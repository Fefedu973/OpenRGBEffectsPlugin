"""Compile pure API policy with MSVC; no Better/network/graphics access."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build" / "better-control-tests"
OUT.mkdir(parents=True, exist_ok=True)
vcvars = Path(os.environ.get("VCVARSALL", r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"))
if not vcvars.is_file():
    raise SystemExit("Set VCVARSALL to the installed x64 MSVC vcvarsall.bat")
batch = OUT / "compile.cmd"
batch.write_text(
    f'@echo off\ncall "{vcvars}" x64 >nul\n'
    f'cd /d "{OUT}"\n'
    f'cl /nologo /std:c++17 /EHsc /W4 /WX /I"{ROOT}" "{Path(__file__).with_name("test_control_state.cpp")}" /Fe:better-control-tests.exe\n'
    'if errorlevel 1 exit /b %errorlevel%\nbetter-control-tests.exe\n', encoding="utf-8")
subprocess.run(["cmd.exe", "/d", "/c", str(batch)], check=True)
