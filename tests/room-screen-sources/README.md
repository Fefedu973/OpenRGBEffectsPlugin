# Screen source tests

The tests compile the production `ScreenSources/ScreenSource.cpp` with Qt Core/Gui and the **unchanged core** `FrameSurface/FrameSurface.h`. They do not create a window, screen capture, WebView or hardware controller. Only generated pixels and randomly process-scoped test channel names are used. One short-lived child of the test executable publishes synthetic pixels; the test terminates only that child to validate producer death.

From an MSVC developer shell:

```bat
set QT_ROOT=C:\path\to\Qt\msvc2022_64
set JOM=C:\path\to\jom.exe
tests\room-screen-sources\Build-Tests.cmd
tests\room-screen-sources\Build-Tests.cmd unsupported
```

`source_tests.pro` defaults `OPENRGB_ROOM_ROOT` to the sibling OpenRGB-Room repository; override it when running qmake for another checkout. The second build compiles the same source with the transport backend disabled, exercising the portable Unsupported behavior without pulling in Windows FrameSurface types.

Covered behavior: invalid configuration; shared subscriptions and last-release join; BGRA ordering and padded stride; QImage copy-on-write and lifetime beyond subsequent frames/source shutdown; latest frame after a 60-frame 800×600 burst; unchanged frame pointer/revision; contention retaining only fresh pixels; TTL enforced by retained snapshots too; prompt stop during contention; dimensions and malformed wire header rejection; later valid frame recovery; normal close and generation reset; actual child-process death and reconnection. Build products remain ignored. These are correctness checks, not end-to-end FPS benchmarks.

Validated on 2026-09-27 with Qt 6.8.3 / MSVC x64: **109 assertions passed** with the Windows backend, and **12 assertions passed** in the separately compiled Unsupported variant. No Better or OpenRGB runtime instance was accessed.
