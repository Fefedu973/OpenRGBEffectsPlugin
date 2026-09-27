# Passive Better integration probe

This short-lived native diagnostic uses production `BetterDiscovery` and
`BetterFrameSource`. It only reads discovery, scenes, generation metadata and
the raw/coverage shared surfaces. It never acquires a lease, sends a mutation,
launches capture, exports an image or dumps credentials, URLs, source geometry
or saved scene names. It is not a bridge or a background service.

Build from an x64 MSVC developer shell (Qt Core/Gui/Network):

```text
python tools/better-native-probe/run.py --qt <Qt-msvc-root> --core <OpenRGB-Room-root>
```

The default only builds into ignored `build/better-native-probe`. Add
`--test-missing` to exercise a nonexistent temporary descriptor without touching
the installed Better configuration. Add `--run` deliberately for the actual
application, optionally with `--descriptor <absolute connection.json path>`.
Do not pass the token as a command argument.

Direct executable use, with Qt DLLs on PATH:

```text
better-native-probe.exe --wait-ms 10000
better-native-probe.exe --descriptor C:\absolute\connection.json --wait-ms 15000
```

It prints one JSON object with aggregate states, dimensions, decimal uint64
generation/sequence strings, metadata schema/revision and remaining pair TTL.
Exit `0` confirms a fresh, generation-matched raw/coverage/metadata snapshot;
exit `2` means unavailable/unusable within the wait; exit `64` means bad CLI
arguments. The wait is bounded to 15 seconds, followed by bounded worker cleanup.
No optical rendering, appearance equivalence or scene-control success is implied.

Validation before live use: compiled with production sources, missing-descriptor
case exits `2` without networking. The production provider's synthetic HTTP and
ORGBFRM1 tests are in `tests/room-better-control` (48 integration checks).
