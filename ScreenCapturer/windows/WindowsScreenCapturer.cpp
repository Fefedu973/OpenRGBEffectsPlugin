/* SPDX-License-Identifier: GPL-2.0-or-later
 * OpenRGB Effects Plugin: Windows Desktop Duplication with CPU readback. */
#include "WindowsScreenCapturer.h"
#include "WindowsCaptureSupport.h"
#include "WindowsCaptureImagePool.h"
#include <QDebug>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QThread>
#include <exception>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace
{
using Microsoft::WRL::ComPtr;
using WindowsCapture::Rotation;
using Clock = std::chrono::steady_clock;
enum class CaptureResult { Frame, Idle, Failed };

class ThreadDpiScope
{
public:
    ThreadDpiScope()
    {
        // Worker-local DPI policy only. Never change process/user DPI settings.
        set_context = reinterpret_cast<SetContext>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext"));
        if(set_context) previous = set_context(reinterpret_cast<HANDLE>(-4)); // PER_MONITOR_AWARE_V2
    }
    ~ThreadDpiScope() { if(set_context && previous) set_context(previous); }
private:
    using SetContext = HANDLE (WINAPI*)(HANDLE);
    SetContext set_context = nullptr;
    HANDLE previous = nullptr;
};

Rotation RotationFromDxgi(DXGI_MODE_ROTATION rotation)
{
    switch(rotation)
    {
    case DXGI_MODE_ROTATION_ROTATE90: return Rotation::Clockwise90;
    case DXGI_MODE_ROTATION_ROTATE180: return Rotation::Clockwise180;
    case DXGI_MODE_ROTATION_ROTATE270: return Rotation::Clockwise270;
    default: return Rotation::Identity;
    }
}

class DxgiCapture
{
public:
    explicit DxgiCapture(WindowsCapture::ImagePool& images) : pool(images) {}
    void Reset()
    {
        pending = false;
        staging.Reset();
        duplication.Reset();
        context.Reset();
        device.Reset();
        width = height = 0;
    }

    HRESULT Open(const QString& name)
    {
        Reset();
        ComPtr<IDXGIFactory1> factory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()));
        if(FAILED(hr)) return hr;
        for(UINT adapter_index = 0; ; ++adapter_index)
        {
            ComPtr<IDXGIAdapter1> adapter;
            hr = factory->EnumAdapters1(adapter_index, adapter.GetAddressOf());
            if(hr == DXGI_ERROR_NOT_FOUND) break;
            if(FAILED(hr)) return hr;
            for(UINT output_index = 0; ; ++output_index)
            {
                ComPtr<IDXGIOutput> output;
                hr = adapter->EnumOutputs(output_index, output.GetAddressOf());
                if(hr == DXGI_ERROR_NOT_FOUND) break;
                if(FAILED(hr)) return hr;
                DXGI_OUTPUT_DESC description{};
                hr = output->GetDesc(&description);
                if(FAILED(hr)) continue;
                if(!description.AttachedToDesktop
                   || name.compare(QString::fromWCharArray(description.DeviceName), Qt::CaseInsensitive) != 0) continue;

                const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
                hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                      D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                      device.GetAddressOf(), nullptr, context.GetAddressOf());
                if(FAILED(hr)) { Reset(); return hr; }
                ComPtr<IDXGIOutput1> output1;
                hr = output.As(&output1);
                if(SUCCEEDED(hr)) hr = output1->DuplicateOutput(device.Get(), duplication.GetAddressOf());
                if(FAILED(hr)) { Reset(); return hr; }
                DXGI_OUTDUPL_DESC duplicate_description{};
                duplication->GetDesc(&duplicate_description);
                rotation = RotationFromDxgi(duplicate_description.Rotation);
                return S_OK;
            }
        }
        return DXGI_ERROR_NOT_FOUND;
    }

    CaptureResult Capture(QImage& image, const std::atomic<bool>& running, HRESULT& error)
    {
        error = S_OK;
        if(pending) return Readback(image, running, error);
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        // Never INFINITE: AcquireNextFrame cannot be cancelled by the caller.
        HRESULT hr = duplication->AcquireNextFrame(16, &info, resource.GetAddressOf());
        if(hr == DXGI_ERROR_WAIT_TIMEOUT) return CaptureResult::Idle;
        if(FAILED(hr)) { error = hr; return CaptureResult::Failed; }
        WindowsCapture::FrameLease<IDXGIOutputDuplication> lease(duplication.Get());
        if(!running.load(std::memory_order_relaxed)) return CaptureResult::Idle;

        ComPtr<ID3D11Texture2D> texture;
        hr = resource.As(&texture);
        if(FAILED(hr)) { error = hr; return CaptureResult::Failed; }
        D3D11_TEXTURE2D_DESC description{};
        texture->GetDesc(&description);
        if(!WindowsCapture::ValidSize(description.Width, description.Height)
           || description.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
        {
            error = DXGI_ERROR_UNSUPPORTED;
            return CaptureResult::Failed;
        }
        if(!staging || width != description.Width || height != description.Height)
        {
            staging.Reset();
            width = description.Width;
            height = description.Height;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.SampleDesc.Count = 1;
            description.SampleDesc.Quality = 0;
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            description.MiscFlags = 0;
            hr = device->CreateTexture2D(&description, nullptr, staging.GetAddressOf());
            if(FAILED(hr)) { error = hr; return CaptureResult::Failed; }
        }
        context->CopyResource(staging.Get(), texture.Get());
        context->Flush(); // submit before nonblocking Map, even on an idle desktop
        hr = lease.Release();
        if(FAILED(hr)) { error = hr; return CaptureResult::Failed; }
        pending = true;
        pending_since = Clock::now();
        return Readback(image, running, error);
    }

private:
    CaptureResult Readback(QImage& image, const std::atomic<bool>& running, HRESULT& error)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto deadline = Clock::now() + std::chrono::milliseconds(4);
        HRESULT hr;
        do
        {
            if(!running.load(std::memory_order_relaxed)) return CaptureResult::Idle;
            hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
            if(hr != DXGI_ERROR_WAS_STILL_DRAWING) break;
            if(Clock::now() >= deadline)
            {
                // Keep only one pending transfer. A stuck GPU is retried via failover.
                if(Clock::now() - pending_since > std::chrono::seconds(1))
                {
                    error = DXGI_ERROR_WAS_STILL_DRAWING;
                    return CaptureResult::Failed;
                }
                return CaptureResult::Idle;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while(true);
        pending = false;
        if(FAILED(hr)) { error = hr; return CaptureResult::Failed; }
        struct Unmap
        {
            ID3D11DeviceContext* context;
            ID3D11Texture2D* texture;
            ~Unmap() { context->Unmap(texture, 0); }
        } unmap{context.Get(), staging.Get()};
        const bool swap_axes = WindowsCapture::SwapsAxes(rotation);
        QImage* owned = pool.Writable(static_cast<int>(swap_axes ? height : width), static_cast<int>(swap_axes ? width : height));
        if(!owned) return CaptureResult::Idle;
        if(!WindowsCapture::CopyBgra(mapped.pData, mapped.RowPitch, width, height, rotation,
                                    owned->bits(), static_cast<std::size_t>(owned->bytesPerLine())))
        {
            error = E_FAIL;
            return CaptureResult::Failed;
        }
        image = *owned; // immutable shared ownership survives Unmap and next capture
        return CaptureResult::Frame;
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> staging;
    WindowsCapture::ImagePool& pool;
    UINT width = 0;
    UINT height = 0;
    Rotation rotation = Rotation::Identity;
    bool pending = false;
    Clock::time_point pending_since{};
};

class GdiCapture
{
public:
    explicit GdiCapture(WindowsCapture::ImagePool& images) : pool(images) {}
    ~GdiCapture() { Reset(); }
    void Reset()
    {
        if(memory_dc && previous_bitmap) SelectObject(memory_dc, previous_bitmap);
        if(bitmap) DeleteObject(bitmap);
        if(memory_dc) DeleteDC(memory_dc);
        if(display_dc) DeleteDC(display_dc);
        display_dc = memory_dc = nullptr;
        bitmap = nullptr;
        previous_bitmap = nullptr;
        pixels = nullptr;
        width = height = 0;
        device_name.clear();
    }

    bool Capture(const QString& name, QImage& image)
    {
        // Native pixels: never combine logical Qt origins with physical sizes.
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if(!EnumDisplaySettingsExW(reinterpret_cast<LPCWSTR>(name.utf16()), ENUM_CURRENT_SETTINGS, &mode, 0)
           || !WindowsCapture::ValidSize(mode.dmPelsWidth, mode.dmPelsHeight))
        {
            Reset();
            return false;
        }
        if(name != device_name || width != mode.dmPelsWidth || height != mode.dmPelsHeight || !bitmap)
        {
            Reset();
            display_dc = CreateDCW(L"DISPLAY", reinterpret_cast<LPCWSTR>(name.utf16()), nullptr, nullptr);
            if(!display_dc) return false;
            memory_dc = CreateCompatibleDC(display_dc);
            if(!memory_dc) { Reset(); return false; }
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(mode.dmPelsWidth);
            info.bmiHeader.biHeight = -static_cast<LONG>(mode.dmPelsHeight);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(display_dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            if(!bitmap || !pixels) { Reset(); return false; }
            previous_bitmap = SelectObject(memory_dc, bitmap);
            if(!previous_bitmap || previous_bitmap == HGDI_ERROR) { previous_bitmap = nullptr; Reset(); return false; }
            device_name = name;
            width = mode.dmPelsWidth;
            height = mode.dmPelsHeight;
        }
        // Display-specific DC is local (0,0), also for monitors left of primary.
        if(!BitBlt(memory_dc, 0, 0, static_cast<int>(width), static_cast<int>(height), display_dc, 0, 0, SRCCOPY)
           || !GdiFlush())
        {
            Reset();
            return false;
        }
        QImage* owned = pool.Writable(static_cast<int>(width), static_cast<int>(height));
        if(!owned) return true; // saturated pool: drop, not an API failure
        if(!WindowsCapture::CopyBgra(pixels, static_cast<std::size_t>(width) * 4, width, height, Rotation::Identity,
                                    owned->bits(), static_cast<std::size_t>(owned->bytesPerLine()))) return false;
        image = *owned;
        return true;
    }

private:
    HDC display_dc = nullptr;
    HDC memory_dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous_bitmap = nullptr;
    void* pixels = nullptr;
    DWORD width = 0;
    DWORD height = 0;
    QString device_name;
    WindowsCapture::ImagePool& pool;
};
}
#endif

WindowsScreenCapturer::WindowsScreenCapturer() : ScreenCapturer() {}
WindowsScreenCapturer::~WindowsScreenCapturer() { Stop(); }

void WindowsScreenCapturer::SetScreen(int screen_index)
{
    if(!qGuiApp) return;
    if(QThread::currentThread() != qGuiApp->thread())
    {
        const QPointer<WindowsScreenCapturer> self(this);
        QMetaObject::invokeMethod(qGuiApp, [self, screen_index] { if(self) self->SetScreen(screen_index); }, Qt::QueuedConnection);
        return;
    }
    const QList<QScreen*> screens = QGuiApplication::screens();
    const QString name = screen_index >= 0 && screen_index < screens.size() ? screens[screen_index]->name() : QString();
    {
        std::lock_guard<std::mutex> guard(target_mutex);
        if(name == device_name) return;
        device_name = name;
        target_revision.fetch_add(1, std::memory_order_relaxed);
    }
    wake.notify_all();
}

void WindowsScreenCapturer::Start()
{
    {
        std::lock_guard<std::mutex> guard(lifecycle_mutex);
        if(capture_thread.joinable()) return;
        continue_capture.store(true, std::memory_order_relaxed);
        capture_thread = std::thread(&WindowsScreenCapturer::CaptureThreadFunction, this);
    }
    emit OnStarted();
}

void WindowsScreenCapturer::Stop()
{
    {
        std::lock_guard<std::mutex> guard(lifecycle_mutex);
        continue_capture.store(false, std::memory_order_relaxed);
        wake.notify_all();
        if(!capture_thread.joinable()) return;
        // Consumers must not destroy/stop the capturer within the direct callback.
        Q_ASSERT(capture_thread.get_id() != std::this_thread::get_id());
        capture_thread.join();
    }
    emit OnStopped();
}

void WindowsScreenCapturer::CaptureThreadFunction()
{
#ifdef _WIN32
    try
    {
        ThreadDpiScope dpi;
        WindowsCapture::ImagePool images;
        DxgiCapture dxgi(images);
        GdiCapture gdi(images);
        WindowsCapture::RetryPolicy retry;
        QString active_name;
        QString last_status;
        std::uint64_t revision = 0;
        auto report = [&](const QString& message, bool error)
        {
            if(last_status == message) return;
            last_status = message;
            qInfo().noquote() << "[WindowsScreenCapturer]" << message;
            if(error) emit OnError(ScreenCapturerError::Other, message);
        };

        while(continue_capture.load(std::memory_order_relaxed))
        {
            const auto started = Clock::now();
            {
                std::lock_guard<std::mutex> guard(target_mutex);
                const auto latest_revision = target_revision.load(std::memory_order_relaxed);
                if(latest_revision != revision)
                {
                    revision = latest_revision;
                    active_name = device_name;
                    dxgi.Reset();
                    gdi.Reset();
                    retry.Reset();
                }
            }
            QImage frame;
            bool fallback_failed = false;
            if(!active_name.isEmpty())
            {
                if(retry.ShouldTryDxgi(started))
                {
                    const HRESULT hr = dxgi.Open(active_name);
                    if(SUCCEEDED(hr))
                    {
                        retry.Opened();
                        gdi.Reset();
                        report(QStringLiteral("DXGI Desktop Duplication (CPU QImage readback)"), false);
                    }
                    else
                    {
                        retry.Failed(Clock::now());
                        report(QStringLiteral("DXGI unavailable (0x%1); GDI fallback, retry in 5 s").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0')), true);
                    }
                }
                if(retry.UsesDxgi())
                {
                    HRESULT hr;
                    if(dxgi.Capture(frame, continue_capture, hr) == CaptureResult::Failed)
                    {
                        dxgi.Reset();
                        retry.Failed(Clock::now());
                        report(QStringLiteral("DXGI frame lost (0x%1); GDI fallback, retry in 5 s").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0')), true);
                    }
                }
                if(!retry.UsesDxgi() && continue_capture.load(std::memory_order_relaxed))
                {
                    fallback_failed = !gdi.Capture(active_name, frame);
                    if(fallback_failed) report(QStringLiteral("Selected display unavailable; capture retries are rate limited"), true);
                }
            }
            if(!frame.isNull() && continue_capture.load(std::memory_order_relaxed)
               && revision == target_revision.load(std::memory_order_relaxed)) emit OnImage(frame);

            auto period = WindowsCapture::FramePeriod(framerate.load(std::memory_order_relaxed), !retry.UsesDxgi());
            if(active_name.isEmpty() || fallback_failed) period = std::chrono::milliseconds(200);
            const auto deadline = std::max(started + period, Clock::now() + std::chrono::milliseconds(1));
            std::unique_lock<std::mutex> guard(target_mutex);
            wake.wait_until(guard, deadline, [&]
            {
                return !continue_capture.load(std::memory_order_relaxed)
                    || revision != target_revision.load(std::memory_order_relaxed);
            });
        }
    }
    catch(const std::exception& error)
    {
        emit OnError(ScreenCapturerError::Other, QStringLiteral("Windows capture stopped: ") + QString::fromUtf8(error.what()));
        continue_capture.store(false, std::memory_order_relaxed);
    }
#endif
}
