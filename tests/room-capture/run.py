"""Build real Windows capture code with Qt/MSVC, run only synthetic/no-target tests."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--qt", type=Path, default=os.environ.get("ROOM_QT"), required=not os.environ.get("ROOM_QT"))
parser.add_argument("--vcvars", type=Path, default=Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvarsall.bat")
args = parser.parse_args()
root = Path(__file__).resolve().parent
build = root / ".build"
build.mkdir(exist_ok=True)
qt = args.qt.resolve()
if not (qt / "bin/qmake.exe").is_file() or not args.vcvars.is_file():
    raise SystemExit("Existing Qt MSVC kit and vcvarsall.bat are required; this runner installs nothing.")
script = build / "build-and-test.cmd"
script.write_text(f'''@echo off
call "{args.vcvars}" x64
if errorlevel 1 exit /b %errorlevel%
set "PATH={qt / 'bin'};%PATH%"
cd /d "{build}"
"{qt / 'bin/qmake.exe'}" "{root / 'capture_tests.pro'}" CONFIG+=release CONFIG-=debug
if errorlevel 1 exit /b %errorlevel%
nmake /nologo
if errorlevel 1 exit /b %errorlevel%
capture-tests.exe
exit /b %errorlevel%
''', encoding="utf-8")
subprocess.run(["cmd.exe", "/d", "/c", str(script)], check=True)
