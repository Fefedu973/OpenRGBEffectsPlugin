/*---------------------------------------------------------*\
| WindowsScreenCapturer.cpp                                 |
|                                                           |
|   OpenRGB Effects Plugin Windows Screen Capturer          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "OpenRGBEffectsPlugin.h"
#include "WindowsScreenCapturer.h"
#include <QGuiApplication>
#include <QPixmap>

#ifdef _WIN32
#include <Windows.h>
#endif

WindowsScreenCapturer::WindowsScreenCapturer() : ScreenCapturer() {}

WindowsScreenCapturer::~WindowsScreenCapturer()
{
    continue_capture = false;

    if(capture_thread != nullptr)
    {
        capture_thread->join();
        delete capture_thread;
        capture_thread = nullptr;
    }
}

void WindowsScreenCapturer::SetScreen(int screen_index)
{
    QList<QScreen*> screens = QGuiApplication::screens();

    if(screen_index >= 0 && screen_index < screens.size())
    {
        screen = screens[screen_index];
    }
    else
    {
        screen = nullptr;
    }
}

void WindowsScreenCapturer::Start()
{
    if(capture_thread == nullptr)
    {
        continue_capture = true;
        capture_thread = new std::thread(&WindowsScreenCapturer::CaptureThreadFunction, this);
    }
}

void WindowsScreenCapturer::Stop()
{
    if(capture_thread != nullptr)
    {
        LOG_VERBOSE("[WindowsScreenCapturer] Stopping capture thread...");
        continue_capture = false;
        capture_thread->join();
        delete capture_thread;
        capture_thread = nullptr;
    }
}

void WindowsScreenCapturer::CaptureThreadFunction()
{
    LOG_VERBOSE("[WindowsScreenCapturer] Thread started");

    while(continue_capture)
    {
        int delay = 1000 / framerate;

        if(screen == nullptr)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto start = std::chrono::steady_clock::now();

#ifdef _WIN32
        emit OnImage(grabWindow(0).toImage());
#endif

        int took = (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        int delta = delay - took;

        // sleep 2 ms minimum
        std::this_thread::sleep_for(std::chrono::milliseconds(delta > 2 ? delta : 2));
    }

    LOG_VERBOSE("[WindowsScreenCapturer] Thread ended");
}

#ifdef _WIN32
extern QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int format = 0);

QPixmap WindowsScreenCapturer::grabWindow(quintptr window) const
{

    QSize windowSize;
    HWND hwnd = reinterpret_cast<HWND>(window);
    if (hwnd)
    {
        RECT r;
        GetClientRect(hwnd, &r);
        windowSize = QSize(r.right - r.left, r.bottom - r.top);
    }
    else
    {
        hwnd = GetDesktopWindow();
        const QRect screenGeometry = screen->geometry();
        windowSize = screenGeometry.size();
        windowSize.setWidth((int)(windowSize.width() * screen->devicePixelRatio()));
        windowSize.setHeight((int)(windowSize.height() * screen->devicePixelRatio()));
    }

    int xIn = 0;
    int yIn = 0;
    int width = windowSize.width();
    int height = windowSize.height();

    // Create and setup bitmap
    HDC display_dc = GetDC(nullptr);
    HDC bitmap_dc = CreateCompatibleDC(display_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(display_dc, width, height);
    HGDIOBJ null_bitmap = SelectObject(bitmap_dc, bitmap);

    // copy data
    HDC window_dc = GetDC(hwnd);
    // remove CAPTUREBLT!!
    BitBlt(bitmap_dc, 0, 0, width, height, window_dc, xIn, yIn, SRCCOPY);

    // clean up all but bitmap
    ReleaseDC(hwnd, window_dc);
    SelectObject(bitmap_dc, null_bitmap);
    DeleteDC(bitmap_dc);
    const QPixmap pixmap = qt_pixmapFromWinHBITMAP(bitmap);
    DeleteObject(bitmap);
    ReleaseDC(nullptr, display_dc);

    return pixmap;
}
#endif
