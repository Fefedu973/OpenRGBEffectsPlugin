"""Build the production source-selection widget with strict I/O boundary doubles; offscreen only."""
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
out = root / "build/better-selection-tests"
out.mkdir(parents=True, exist_ok=True)
includes = [root, args.core / "dependencies/json", root / "Effects/Shaders", root / "ScreenCapturer",
            root / "ScreenCapturer/windows", root / "ScreenCapturer/qt", args.qt / "include"]
includes += [args.qt / "include" / ("Qt" + module) for module in ("Core", "Gui", "Widgets", "OpenGL")]
moc = out / "moc_ScreenCapturer.cpp"
subprocess.run([str(args.qt / "bin/moc.exe"), str(root / "ScreenCapturer/ScreenCapturer.h"), "-o", str(moc)], check=True)
command = ["cl", "/nologo", "/EHsc", "/std:c++17", "/Zc:__cplusplus", "/permissive-", "/MD", "/O2", "/DNOMINMAX", "/utf-8"]
sources = [Path(__file__).with_name("selection_tests.cpp"), root / "Effects/SignalFavorites/ScreenSourceSelection.cpp", moc]
command += ["/I" + str(path) for path in includes] + list(map(str, sources))
command += ["/Fo:" + str(out) + os.sep, "/Fe:" + str(out / "selection_tests.exe"), "/link",
            "/LIBPATH:" + str(args.qt / "lib"), "Qt6Core.lib", "Qt6Gui.lib", "Qt6Widgets.lib", "Qt6OpenGL.lib"]
subprocess.run(command, cwd=out, check=True)
env = dict(os.environ, PATH=str(args.qt / "bin") + os.pathsep + os.environ["PATH"], QT_QPA_PLATFORM="offscreen")
subprocess.run([str(out / "selection_tests.exe")], env=env, check=True, timeout=30)
