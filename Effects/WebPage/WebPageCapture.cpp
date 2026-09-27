/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WebPageCapture.h"
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QBuffer>
#include <QImageReader>
#include <QElapsedTimer>
#include <algorithm>

#if defined(_WIN32) && defined(ROOM_WEBVIEW2)
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <wrl.h>
#include <WebView2.h>
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
#endif

bool WebPageCapture::ValidUrl(const QUrl& url)
{
    if(!url.isValid() || url.toString().size() > 8192 || !url.userInfo().isEmpty()) return false;
    if(url.scheme() == "http" || url.scheme() == "https") return !url.host().isEmpty();
    return url.scheme() == "file" && url.isLocalFile() && url.host().isEmpty()
        && QDir::isAbsolutePath(url.toLocalFile()); // No UNC/network file shares.
}

bool WebPageCapture::ValidSize(int w, int h)
{
    return w >= 16 && h >= 16 && w <= 4096 && h <= 4096 && quint64(w) * h <= 4096ULL * 4096;
}

QImage WebPageCapture::DecodePng(const QByteArray& bytes, int w, int h)
{
    const QByteArray signature("\x89PNG\r\n\x1a\n",8);
    if(!ValidSize(w,h) || !bytes.startsWith(signature) || bytes.size() > 80*1024*1024) return {};
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "PNG");
    // Reject dimensions before decoding: screenshots have exactly the viewport.
    if(reader.size() != QSize(w,h)) return {};
    const QImage image = reader.read();
    return image.isNull() ? QImage() : image.convertToFormat(QImage::Format_RGB32);
}

struct WebPageCapture::State : std::enable_shared_from_this<State>
{
    QPointer<WebPageCapture> owner;
    QTimer timer;
    QElapsedTimer request_time;
    bool running = true, pending = false, loaded = false, first_navigation_done = false;
    bool navigation_retry_pending = false;
    unsigned navigation_failures = 0;
    quint64 navigation_id = 0;
    int width = 0, height = 0;
    QUrl url;
    std::unique_ptr<QTemporaryDir> profile;
#if defined(_WIN32) && defined(ROOM_WEBVIEW2)
    HWND window = nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> browser;
    bool com_initialized = false;
    ~State() { Close(); if(com_initialized) CoUninitialize(); }
    void Close()
    {
        running = false;
        timer.stop();
        if(browser) browser->Stop();
        if(controller) controller->Close();
        browser.Reset(); controller.Reset(); environment.Reset();
        if(window) { DestroyWindow(window); window = nullptr; }
    }
    void Fail(const char* operation, HRESULT hr)
    {
        if(owner && running) emit owner->Status(QString("%1 failed (0x%2)").arg(operation).arg(quint32(hr),8,16,QLatin1Char('0')));
        Close();
    }
    void Capture()
    {
        if(!running || !loaded || !browser) return;
        if(pending)
        {
            if(request_time.isValid() && request_time.elapsed()>5000) Fail("Capture timeout",HRESULT_FROM_WIN32(ERROR_TIMEOUT));
            return;
        }
        ComPtr<IStream> stream;
        HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        if(FAILED(hr)) { Fail("Create capture stream",hr); return; }
        pending = true;
        request_time.start();
        const auto keep = shared_from_this();
        hr = browser->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
            Callback<ICoreWebView2CapturePreviewCompletedHandler>([keep,stream](HRESULT result)->HRESULT
            {
                keep->pending = false;
                if(!keep->running || !keep->owner) return S_OK;
                if(FAILED(result)) { keep->Fail("CapturePreview",result); return S_OK; }
                STATSTG stat{};
                if(FAILED(stream->Stat(&stat,STATFLAG_NONAME)) || stat.cbSize.HighPart || stat.cbSize.LowPart > 80*1024*1024)
                { keep->Fail("Capture bounds",E_FAIL); return S_OK; }
                QByteArray bytes(int(stat.cbSize.LowPart),Qt::Uninitialized);
                LARGE_INTEGER origin{};
                ULONG read = 0;
                if(FAILED(stream->Seek(origin,STREAM_SEEK_SET,nullptr)) || FAILED(stream->Read(bytes.data(),ULONG(bytes.size()),&read)) || read != ULONG(bytes.size()))
                { keep->Fail("Read capture",E_FAIL); return S_OK; }
                QImage image = DecodePng(bytes,keep->width,keep->height);
                if(image.isNull()) { keep->Fail("Decode capture",E_FAIL); return S_OK; }
                emit keep->owner->FrameReady(image);
                return S_OK;
            }).Get());
        if(FAILED(hr)) { pending=false; Fail("CapturePreview",hr); }
    }
    HRESULT Install(ICoreWebView2Controller* incoming)
    {
        controller = incoming;
        HRESULT hr = controller->get_CoreWebView2(&browser);
        if(FAILED(hr)) return hr;
        RECT bounds{0,0,width,height};
        if(FAILED(hr=controller->put_Bounds(bounds))) return hr;
        ComPtr<ICoreWebView2Controller3> scale;
        if(SUCCEEDED(controller.As(&scale)))
        {
            scale->put_ShouldDetectMonitorScaleChanges(FALSE);
            scale->put_RasterizationScale(1.0);
            scale->put_BoundsMode(COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);
        }
        if(FAILED(hr=controller->put_IsVisible(TRUE))) return hr;
        ComPtr<ICoreWebView2Settings> settings;
        if(FAILED(hr=browser->get_Settings(&settings))) return hr;
        if(FAILED(hr=settings->put_IsWebMessageEnabled(FALSE))) return hr;
        if(FAILED(hr=settings->put_AreHostObjectsAllowed(FALSE))) return hr;
        if(FAILED(hr=settings->put_AreDevToolsEnabled(FALSE))) return hr;
        if(FAILED(hr=settings->put_AreDefaultContextMenusEnabled(FALSE))) return hr;
        if(FAILED(hr=settings->put_AreDefaultScriptDialogsEnabled(FALSE))) return hr;
        if(FAILED(hr=settings->put_IsStatusBarEnabled(FALSE))) return hr;
        ComPtr<ICoreWebView2Settings3> settings3;
        if(SUCCEEDED(settings.As(&settings3))) settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
        ComPtr<ICoreWebView2_8> audio;
        if(SUCCEEDED(browser.As(&audio))) audio->put_IsMuted(TRUE);
        EventRegistrationToken token{};
        const std::weak_ptr<State> weak = shared_from_this();
        hr=browser->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
            [weak](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* args)->HRESULT
            {
                LPWSTR value=nullptr; args->get_Uri(&value);
                const QUrl target(QString::fromWCharArray(value ? value : L"")); CoTaskMemFree(value);
                const auto s=weak.lock();
                if(!s || !s->running || !ValidUrl(target) || (s->url.scheme() != "file" && target.scheme()=="file")) args->put_Cancel(TRUE);
                if(s) { s->loaded=false; args->get_NavigationId(&s->navigation_id); }
                return S_OK;
            }).Get(),&token);
        if(FAILED(hr)) return hr;
        browser->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [weak](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* args)->HRESULT
            {
                const auto s=weak.lock(); if(!s || !s->running) return S_OK;
                UINT64 completed_id=0; args->get_NavigationId(&completed_id);
                if(completed_id!=s->navigation_id) return S_OK;
                s->first_navigation_done=true;
                BOOL ok=FALSE; args->get_IsSuccess(&ok); s->loaded=ok;
                if(ok)
                {
                    s->navigation_failures=0;
                    if(s->owner) emit s->owner->Status("Rendering web page");
                }
                else if(s->url.scheme()=="http" || s->url.scheme()=="https")
                {
                    // A local capture server may start after OpenRGB. Retry the
                    // configured URL; never relax TLS or redirect validation.
                    if(s->owner) emit s->owner->Status("Page unavailable; retrying automatically");
                    if(!s->navigation_retry_pending && s->owner)
                    {
                        s->navigation_retry_pending=true;
                        const unsigned delay=std::min(15000u,1000u<<std::min(s->navigation_failures++,4u));
                        QTimer::singleShot(int(delay),s->owner.data(),[weak]
                        {
                            const auto retry=weak.lock();
                            if(!retry || !retry->running || !retry->browser) return;
                            retry->navigation_retry_pending=false;
                            if(retry->loaded) return;
                            const auto address=retry->url.toString(QUrl::FullyEncoded).toStdWString();
                            const HRESULT result=retry->browser->Navigate(address.c_str());
                            if(FAILED(result)) retry->Fail("Retry navigation",result);
                        });
                    }
                }
                else if(s->owner) emit s->owner->Status("Page navigation failed; no new frame");
                return S_OK;
            }).Get(),&token);
        if(FAILED(hr=browser->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* args)->HRESULT { return args->put_Handled(TRUE); }).Get(),&token))) return hr;
        if(FAILED(hr=browser->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [](ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs* args)->HRESULT { return args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); }).Get(),&token))) return hr;
        ComPtr<ICoreWebView2_4> downloads;
        if(FAILED(browser.As(&downloads))) return E_NOINTERFACE;
        if(FAILED(hr=downloads->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
            [](ICoreWebView2*,ICoreWebView2DownloadStartingEventArgs* args)->HRESULT { args->put_Handled(TRUE); return args->put_Cancel(TRUE); }).Get(),&token))) return hr;
        ComPtr<ICoreWebView2_18> external;
        if(FAILED(browser.As(&external))) return E_NOINTERFACE;
        if(FAILED(hr=external->add_LaunchingExternalUriScheme(Callback<ICoreWebView2LaunchingExternalUriSchemeEventHandler>(
            [](ICoreWebView2*,ICoreWebView2LaunchingExternalUriSchemeEventArgs* args)->HRESULT { return args->put_Cancel(TRUE); }).Get(),&token))) return hr;
        browser->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>(
            [weak](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*)->HRESULT
            { if(auto s=weak.lock()) s->Fail("Browser process",E_FAIL); return S_OK; }).Get(),&token);
        // No mouse/keyboard events are forwarded to the off-screen host. File
        // inputs additionally remain disabled; no host filesystem API is exposed.
        browser->AddScriptToExecuteOnDocumentCreated(L"(()=>{const block=()=>document.querySelectorAll('input[type=file]').forEach(e=>{e.disabled=true});new MutationObserver(block).observe(document,{subtree:true,childList:true});document.addEventListener('DOMContentLoaded',block);})();",nullptr);
        const auto address=url.toString(QUrl::FullyEncoded).toStdWString();
        return browser->Navigate(address.c_str());
    }
#else
    void Close() { running=false; timer.stop(); }
#endif
};

WebPageCapture::WebPageCapture(QObject* parent):QObject(parent) {}
WebPageCapture::~WebPageCapture() { Stop(); }

void WebPageCapture::Stop()
{
    Q_ASSERT(QThread::currentThread()==thread());
    if(state) { state->owner=nullptr; state->Close(); state.reset(); }
}

void WebPageCapture::Start(const QUrl& url,int width,int height,int fps)
{
    Q_ASSERT(QThread::currentThread()==thread());
    Stop();
    if(!ValidUrl(url) || !ValidSize(width,height) || fps<1 || fps>30)
    { emit Status("Invalid URL, canvas dimensions or frame rate"); return; }
#if defined(_WIN32) && defined(ROOM_WEBVIEW2)
    auto next=std::make_shared<State>(); state=next;
    next->owner=this; next->url=url; next->width=width; next->height=height;
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)) { next->Fail("WebView2 GUI STA",initialized); return; }
    next->com_initialized=true;
    // Load only beside this plugin/test executable: never from PATH or cwd.
    HMODULE own=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&WebPageCapture::ValidSize),&own);
    wchar_t module_path[32768]{}; GetModuleFileNameW(own,module_path,32768);
    const QString loader_path=QFileInfo(QString::fromWCharArray(module_path)).dir().filePath("WebView2Loader.dll");
    // Loader remains resident for outstanding asynchronous COM callbacks.
    static HMODULE loader=nullptr;
    if(!loader) loader=LoadLibraryExW(reinterpret_cast<LPCWSTR>(loader_path.utf16()),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    using Create=HRESULT(STDAPICALLTYPE*)(PCWSTR,PCWSTR,ICoreWebView2EnvironmentOptions*,ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    const auto create=loader ? reinterpret_cast<Create>(GetProcAddress(loader,"CreateCoreWebView2EnvironmentWithOptions")) : nullptr;
    if(!create) { next->Fail("WebView2Loader.dll unavailable",HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)); return; }
    // COM initialization/capture completions cannot be cancelled synchronously.
    // Keep callback code mapped even if a nonstandard host unloads the plugin
    // while a completion is pending. This pins code, NOT a running browser.
    static HMODULE callback_module=nullptr;
    if(!callback_module && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&WebPageCapture::ValidSize),&callback_module))
    { next->Fail("Pin asynchronous callback module",HRESULT_FROM_WIN32(GetLastError())); return; }
    next->profile.reset(new QTemporaryDir(QDir::tempPath()+"/OpenRGB-WebPage-XXXXXX"));
    if(!next->profile->isValid()) { next->Fail("Isolated browser profile",E_FAIL); return; }
    next->window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"OpenRGB WebPage capture",WS_POPUP,
                               -32000,-32000,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!next->window) { next->Fail("WebPage host",HRESULT_FROM_WIN32(GetLastError())); return; }
    ShowWindow(next->window,SW_SHOWNOACTIVATE);
    next->timer.setInterval(std::max(1,(1000+fps-1)/fps));
    next->timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&next->timer,&QTimer::timeout,this,[weak=std::weak_ptr<State>(next)]{ if(auto s=weak.lock()) s->Capture(); });
    const auto folder=next->profile->path().toStdWString();
    emit Status("Starting isolated WebView2 renderer");
    QTimer::singleShot(20000,this,[weak=std::weak_ptr<State>(next)]
    {
        if(auto s=weak.lock())
            if(s->running && !s->first_navigation_done) s->Fail("Page startup timeout",HRESULT_FROM_WIN32(ERROR_TIMEOUT));
    });
    const HRESULT result=create(nullptr,folder.c_str(),nullptr,Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [next](HRESULT hr,ICoreWebView2Environment* environment)->HRESULT
        {
            if(!next->running || !next->owner) return S_OK;
            if(FAILED(hr) || !environment) { next->Fail("WebView2 runtime",FAILED(hr)?hr:E_FAIL); return S_OK; }
            next->environment=environment;
            hr=environment->CreateCoreWebView2Controller(next->window,Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [next](HRESULT result,ICoreWebView2Controller* controller)->HRESULT
                {
                    if(!next->running || !next->owner) { if(controller) controller->Close(); return S_OK; }
                    if(FAILED(result) || !controller) { next->Fail("WebView2 controller",FAILED(result)?result:E_FAIL); return S_OK; }
                    const HRESULT installed=next->Install(controller);
                    if(FAILED(installed)) next->Fail("Configure WebView2",installed); else next->timer.start();
                    return S_OK;
                }).Get());
            if(FAILED(hr)) next->Fail("Create controller",hr);
            return S_OK;
        }).Get());
    if(FAILED(result)) next->Fail("Create environment",result);
#else
    emit Status("WebPage requires Windows and the optional WebView2 SDK/runtime");
#endif
}
