"""Build/run native inference checks from an MSVC x64 developer environment."""
import argparse
import os
from pathlib import Path
import subprocess
import inference_fixtures

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--qt", required=True, type=Path)
    p.add_argument("--core", required=True, type=Path)
    p.add_argument("--ort", required=True, type=Path, help="Extracted official onnxruntime-win-x64-1.30.0 directory")
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = root / "build" / "intelligent-ambience-inference"
    out.mkdir(parents=True, exist_ok=True)
    fixtures = inference_fixtures.generate(out / "fixtures")
    source = root / "Effects" / "IntelligentAmbience" / "Inference"
    command = ["cl", "/nologo", "/std:c++17", "/Zc:__cplusplus", "/EHsc", "/O2", "/W4", "/MD", "/utf-8", "/permissive-", "/DNOMINMAX",
               f"/I{source}", f"/I{a.qt / 'include'}", f"/I{a.qt / 'include/QtCore'}", f"/I{a.core / 'dependencies/json'}",
               str(source / "InferenceWorker.cpp"), str(Path(__file__).with_name("inference_tests.cpp")),
               f"/Fe:{out / 'inference_tests.exe'}", "/link", f"/LIBPATH:{a.qt / 'lib'}", "Qt6Core.lib"]
    subprocess.run(command, cwd=out, check=True)
    env = dict(os.environ)
    env["PATH"] = str(a.qt / "bin") + os.pathsep + env.get("PATH", "")
    subprocess.run([str(out / "inference_tests.exe"), str((a.ort / "lib/onnxruntime.dll").resolve()), str(fixtures)], env=env, cwd=out, check=True, timeout=60)

if __name__ == "__main__": main()
