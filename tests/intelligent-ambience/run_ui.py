"""Build/run the real Effects DLL in an empty, temporary API5 test host.

The only started effect uses its synthetic demo, with audio disabled and an
unavailable temporary Better descriptor. No physical controller is supplied.
Run from an x64 MSVC environment matching the supplied Qt and Effects DLL.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
for argument in ("qt", "jom", "core", "dll"):
    parser.add_argument("--" + argument, type=Path, required=True)
mode = parser.add_mutually_exclusive_group()
mode.add_argument("--build-only", action="store_true")
mode.add_argument("--run-only", action="store_true")
args = parser.parse_args()
here = Path(__file__).resolve().parent
out = here.parents[1] / "build/intelligent-ambience-ui"
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, PATH=str(args.qt.resolve() / "bin") + os.pathsep + os.environ["PATH"])
# Qt's offscreen platform is adequate for widgets but not the native Windows GL
# worker. Widgets use WA_DontShowOnScreen; this does not show a physical window.
if os.name == "nt":
    env["QT_QPA_PLATFORM"] = "windows"
if not args.run_only:
    subprocess.run([str(args.qt.resolve() / "bin/qmake.exe"), str(here / "ui_effect_test.pro"),
                    "CONFIG+=release", "CONFIG-=debug_and_release", "CONFIG-=build_all",
                    "OPENRGB_ROOM_ROOT=" + args.core.resolve().as_posix()],
                   cwd=out, env=env, check=True)
    subprocess.run([str(args.jom.resolve()), "/J", "4"], cwd=out, env=env, check=True)
if not args.build_only:
    subprocess.run([str(out / "intelligent-ambience-ui-test.exe"), str(args.dll.resolve())],
                   cwd=out, env=env, check=True, timeout=90)
