"""Build/test MusicDirector using synthetic observations only. MSVC developer shell."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
output = root / 'build' / 'intelligent-ambience-music'
output.mkdir(parents=True, exist_ok=True)
subprocess.run([
    'cl', '/nologo', '/EHsc', '/std:c++17', '/W4', '/O2', '/MD', '/DNOMINMAX', '/utf-8',
    '/I' + str(root), str(root / 'Effects/IntelligentAmbience/MusicDirector.cpp'),
    str(Path(__file__).with_name('music_director_tests.cpp')),
    '/Fe:' + str(output / 'music_director_tests.exe')
], cwd=output, check=True)
subprocess.run([str(output / 'music_director_tests.exe')], cwd=output, check=True, timeout=30)
