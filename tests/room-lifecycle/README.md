The native core dispatches deferred virtual-controller registration on the Qt
application thread. Its device-list notification calls plugins synchronously.
Effects must remap directly when already on its owning thread; worker callers
still require blocking delivery so old controller descriptions remain valid
until the remap completes.

From an x64 MSVC developer prompt:

```powershell
python tests/room-lifecycle/run.py --qt C:\Qt\6.8.3\msvc2022_64
python tests/room-lifecycle/run.py --qt C:\Qt\6.8.3\msvc2022_64 --without-fix
```

The fixture extracts the current production callback, stubs only the
`UpdateControllers` body, and uses actual Qt dispatch and a real worker thread.
It checks same-thread synchronous completion, worker execution on the GUI
thread and unrelated-event filtering. Depending on Qt version, the former
self-blocking callback is rejected without a remap, or hangs and is terminated
by the five-second test-process deadline. Qt 6.8.3 here rejects it: zero remaps.
No OpenRGB instance, devices or plugin DLL is opened.
