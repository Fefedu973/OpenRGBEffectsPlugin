"""Build the passive Better native probe. Default is build-only, never live I/O."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--qt", type=Path, required=True)
parser.add_argument("--core", type=Path, required=True)
parser.add_argument("--run", action="store_true", help="Explicitly run against the current Better descriptor")
parser.add_argument("--descriptor", type=Path, help="Absolute descriptor path; never a credential argument")
parser.add_argument("--wait-ms", type=int, default=10000)
parser.add_argument("--test-missing", action="store_true", help="Only probe a nonexistent descriptor in a temporary directory")
args = parser.parse_args()
if not 100 <= args.wait_ms <= 15000:
    parser.error("--wait-ms must be within 100..15000")
if args.descriptor is not None and not args.descriptor.is_absolute():
    parser.error("--descriptor must be absolute")
if args.test_missing and (args.run or args.descriptor):
    parser.error("--test-missing cannot be combined with --run or --descriptor")
root = Path(__file__).resolve().parents[2]
out = root / "build/better-native-probe"
out.mkdir(parents=True, exist_ok=True)
exe = out / "better-native-probe.exe"
include = [root, args.core, args.qt / "include"] + [args.qt / "include" / ("Qt" + m) for m in ("Core", "Gui", "Network")]
sources = [root / "tools/better-native-probe.cpp"] + [root / "ScreenSources" / (name + ".cpp") for name in ("BetterDiscovery", "BetterFrameSource", "ScreenSource")]
command = ["cl", "/nologo", "/EHsc", "/std:c++17", "/Zc:__cplusplus", "/permissive-", "/MD", "/O2", "/DNOMINMAX", "/utf-8"]
command += ["/I" + str(p) for p in include] + list(map(str, sources))
command += ["/Fo:" + str(out) + os.sep, "/Fe:" + str(exe), "/link", "/LIBPATH:" + str(args.qt / "lib"), "Qt6Core.lib", "Qt6Gui.lib", "Qt6Network.lib", "advapi32.lib"]
subprocess.run(command, cwd=out, check=True)
env = dict(os.environ, PATH=str(args.qt / "bin") + os.pathsep + os.environ["PATH"])
if args.test_missing:
    with tempfile.TemporaryDirectory(prefix="better-passive-probe-") as directory:
        result = subprocess.run([str(exe), "--descriptor", str(Path(directory) / "absent.json"), "--wait-ms", "200"],
                                env=env, capture_output=True, text=True, timeout=3)
        value = json.loads(result.stdout)
        assert result.returncode == 2 and value["ready"] is False and value["read_only"] is True
        assert value["discovery_status"] == "missing_descriptor" and value["elapsed_ms"] < 1500
        assert not any(key in value for key in ("token", "baseUrl", "image", "lease"))
        print("PASS: missing descriptor is bounded, read-only and unavailable (exit 2)")
elif args.run:
    command = [str(exe), "--wait-ms", str(args.wait_ms)]
    if args.descriptor is not None:
        command += ["--descriptor", str(args.descriptor)]
    raise SystemExit(subprocess.run(command, env=env, timeout=18).returncode)
else:
    print("Built diagnostic; no probe executed:", exe)
