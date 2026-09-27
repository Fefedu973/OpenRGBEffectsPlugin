"""Build actual worker + HTTP client + ORGBFRM1, using synthetic sources only."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--qt", type=Path, required=True)
parser.add_argument("--core", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / "build/better-control-tests"
out.mkdir(parents=True, exist_ok=True)
include = [root, args.core, args.qt / "include"] + [args.qt / "include" / ("Qt" + m) for m in ("Core", "Gui", "Network")]
sources = [Path(__file__).with_name("frame_source_tests.cpp")] + [root / "ScreenSources" / (name + ".cpp") for name in ("BetterDiscovery", "BetterFrameSource", "ScreenSource")]
command = ["cl", "/nologo", "/EHsc", "/std:c++17", "/Zc:__cplusplus", "/permissive-", "/MD", "/O2", "/DNOMINMAX", "/utf-8"]
command += ["/I" + str(p) for p in include] + list(map(str, sources))
command += ["/Fo:" + str(out) + os.sep, "/Fe:" + str(out / "frame_source_tests.exe"), "/link", "/LIBPATH:" + str(args.qt / "lib"), "Qt6Core.lib", "Qt6Gui.lib", "Qt6Network.lib", "advapi32.lib"]
subprocess.run(command, cwd=out, check=True)
env = dict(os.environ, PATH=str(args.qt / "bin") + os.pathsep + os.environ["PATH"])
subprocess.run([str(out / "frame_source_tests.exe")], env=env, check=True, timeout=35)
