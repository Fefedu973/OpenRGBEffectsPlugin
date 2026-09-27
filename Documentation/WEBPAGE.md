# Web Page: URL to canvas, native images and LED samples

`Web Page` is a Special effect. On Windows it renders an HTTP, HTTPS or local
`file:///C:/...` page using the installed Microsoft Edge WebView2 Evergreen
runtime. It starts only when the effect is enabled. Disabling the effect closes
its browser controller and releases its environment; there is no manually
opened browser window and no QtWebEngine dependency.

Enter the URL, canvas size and capture limit, then select **Apply / reload page**.
The default canvas is 800×600 at a maximum of 20 captures/s; dimensions are
16–4096 each (at most 16,777,216 pixels), and the capture limit is 1–30 FPS.
This is an upper limit, not a performance guarantee. CapturePreview encodes PNG
in WebView2 and the callback decodes an owned CPU QImage on the GUI thread.
Large pages and large images cost more CPU and memory. The path is not zero-copy
and does not expose a GPU texture API.

The canvas does not create one LED per pixel. It uses the existing generic
`CanvasRouting` path: native image-capable whole zones receive a shared BGRA
frame and affine mapping, while ordinary zones/segments receive their LED
samples. Brightness, temperature and tint are applied by the same helpers as
Ambient and Shaders. `zone_regions` uses the same selector, normalized rectangle,
rotation and flip schema documented in `ROOM-CANVAS-ARCHITECTURE.md`.
Optional FrameSurface publication uses a configurable channel (`room-webpage`
by default); no device name or specific screen is hardcoded.

## Browser boundaries and lifecycle

- Qt GUI/STA owns every COM call; capture never starts from the effect worker.
- One asynchronous capture can be in flight. The consumer keeps only the latest
  image; it does not queue every browser frame. A stalled capture fails after
  five seconds. Initial page loading is bounded to twenty seconds.
- A fresh, isolated temporary profile is used per start/reload. It never reads
  the user's Edge profile. Stop closes the controller; pending callbacks retain
  only their state and check cancellation/QPointer before touching Qt objects.
  Once WebPage is first used, the plugin code module is pinned until process
  exit so even an alternative plugin host cannot unmap pending COM callbacks.
  Browser processes still close normally when the effect stops. Replacing this
  DLL after using WebPage therefore requires restarting OpenRGB.
  Temporary-directory removal is best effort if Chromium still holds a file
  briefly after closing; it is not an assertion of secure disk erasure.
- No host objects or web messages are exposed. Permissions, downloads, popups,
  external URI launches, context menus, browser hotkeys and default script
  dialogs are disabled. No keyboard/mouse input is forwarded. File-upload
  inputs are additionally disabled by a document script. Ordinary page scripts,
  HTTP requests, embedded resources and same-window redirects continue to work.
  This is a webpage renderer, not an offline/network sandbox; use trusted URLs.
- Local files require an absolute path and no UNC host. HTTP pages cannot
  navigate to a `file:` URL. Usernames/passwords embedded in the URL are refused.
- Frames older than two seconds stop being routed. Native image leases and
  FrameSurface TTLs then expire according to the consuming output. Ordinary
  LED-only devices hold their last color; no unrelated power state is changed.

Reload applies the whole URL/size/FPS tuple atomically. The old browser is stopped
first; late captures from it cannot be installed in the new canvas. Closing the
effect cancels queued GUI restarts through the QObject context. There is no
worker thread to terminate and no forced browser-process kill.

## Building and distributing

1. Run `python tools/fetch-webview2.py`. It downloads the official
   `Microsoft.Web.WebView2` NuGet SDK **1.0.4191.47**, checks its pinned SHA-256,
   and extracts it under ignored `build/webview2-sdk`.
2. Build Effects normally. `Effects/WebPage/WebPage.pri` detects the SDK. An
   external SDK location can be passed as `WEBVIEW2_SDK=<folder>` to qmake.
3. Place the architecture-matching `build/native/x64/WebView2Loader.dll`
   (or x86 for a 32-bit build) **beside OpenRGBEffectsPlugin.dll**. Copy the SDK's
   `LICENSE.txt` as `WebView2-LICENSE.txt` beside it. The loader is deliberately
   resolved only beside the plugin, never through the working directory/PATH.
4. The machine must have the WebView2 Evergreen runtime. It is not included or
   installed by this repository. Missing SDK, loader or runtime leaves the
   effect unavailable with an explicit status; other effects remain usable.

The new effect sources are GPL-2.0-or-later with the Effects repository. The
Microsoft SDK/loader uses its own BSD-style redistribution terms; retain its
copyright notice, conditions and disclaimer. No Microsoft browser binaries,
private profiles or browsing data belong in the source repository.

## Validation (27 September 2026)

MSVC x64 / Qt 6.8.3 compilation and full Effects DLL linking passed. On installed
WebView2 runtime 153.0.4234.48, the standalone test rendered three 800×600 local
HTML gradient frames and six HTTP loopback frames animated by JavaScript
`requestAnimationFrame`, verified their actual pixels, changed the URL by stopping/restarting, cancelled a separate
initialization before completion, and observed no frames during each 500 ms
stop window. **61 assertions passed**, including URL bounds, PNG dimensions,
generic native image routing and 15 LED samples. A 20 ms GUI heartbeat observed
a maximum gap of 128 ms during initialization/capture in that run. This is a
bounded 800×600 fixture result, not a benchmark for arbitrary pages, 4K frames,
or hundreds of downstream outputs. No hardware output was opened.

Build `tests/room-webpage/webpage-tests.pro`, copy the matching loader beside the
test executable, and run `webpage_tests` for parser/routing checks or
`webpage_tests --browser` for local file/HTTP rendering and lifecycle checks.
The browser test has a 45-second timeout and uses only temporary fixture pages
and an ephemeral loopback HTTP server.

Primary references:

- [WebView2 threading model](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/threading-model)
- [Win32 WebView2 interface and CapturePreview](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2)
- [Local content](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/working-with-local-content)
- [Official NuGet package](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4191.47)
