# SDK and profile GUI dispatch

The core's `NetworkServer` delivers packet 201 (plugin-specific commands) to
`PluginManager::OnSDKCommand` on the network thread. Effects command 20/21 then
used to traverse widgets and start/stop effects on that thread. Effect-list
responses and `OnProfileSave` also read widgets directly. This became particularly
unsafe for effects whose activation owns screen capture or input controls.

`OpenRGBEffectsPlugin.cpp` now dispatches these operations, profile About/Load,
and Unload to the **widget's** owning thread. Calls already on that thread execute
directly; other callers wait for the result/order using Qt's blocking queued
delivery. There is no new mutex, event loop, profile transaction, or state reload.
Profile autosave only collects current settings. Exceptions raised by an invoked
operation return to the caller rather than escaping Qt's event dispatcher.

The host's `SettingsManager::SignalSettingsManagerUpdate` holds its callback-list
mutex while calling plugins. Its language refresh is therefore **posted without
waiting**, with the widget as lifetime context. Existing queued controller-update
callbacks remain unchanged. The inspected host profile save and SDK dispatch
paths do not hold a GUI callback mutex across the plugin call.

SDK effect-name strings remain length-prefixed and NUL-terminated. Missing,
truncated or malformed strings are ignored before any widget access; a copied
name is used for dispatch. The list response size starts at zero instead of
including arbitrary request bytes. Protocol version and API 5 vtable do not change.

## Test

From an MSVC x64 developer shell:

```powershell
python tests/room-sdk-dispatch/run.py --qt C:/Qt/6.8.3/msvc2022_64 --core ../OpenRGB-Room
python tests/room-sdk-dispatch/run.py --qt C:/Qt/6.8.3/msvc2022_64 --core ../OpenRGB-Room --without-marshalling
```

The runner extracts the actual production dispatch helper and six hook bodies,
then compiles them with Qt and an instrumented UI stand-in. It checks GUI and
network callers, ordered start/list/stop, synchronous profile capture, a GUI
checkpoint timer running alongside network requests, exception relay, malformed
buffers and posted callback destruction. Deliberately different plugin/widget
affinities ensure the receiver determines the target thread. The settings-worker
test returns while the GUI waits for the caller, exposing any blocking callback
regression. The negative mode requires direct dispatch to fail its affinity check.

Validated with Qt 6.8.3/MSVC: 675 assertions in the first positive run (the count
varies with checkpoint timer scheduling), plus the negative regression. No plugin
load, screen/input capture, application restart, network connection, or hardware
operation is performed. Like the existing About/Load hooks, synchronous calls
require the GUI event loop to be alive; this patch does not redesign concurrent
host plugin unloading or make a blocked GUI responsive.
