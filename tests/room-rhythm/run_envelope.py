"""Pure C++ renderer-envelope tests plus a targeted null-Rhythm source guard.
No application, audio endpoint, GPU or hardware is opened.
"""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
output=root/'build/room-rhythm'
output.mkdir(parents=True,exist_ok=True)
source=(root/'Effects/Shaders/ShaderRenderer.cpp').read_text(encoding='utf-8')
method=source.split('void ShaderRenderer::UpdateUniforms(',1)[1].split('void ShaderRenderer::UpdateCustomUniforms',1)[0]
legacy=method.index('uniforms.iMusic = music_envelope.Update(')
guard=method.index('if(rhythm)')
assert legacy<guard, 'Legacy iMusic update must remain unconditional before optional rhythm'
assert method.index('uniforms.iRhythm = {}; uniforms.iOnset = {};')<guard
assert method.index('rhythm_envelope.Update(*rhythm,seconds)')>guard
subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/O2','/MD','/DNOMINMAX','/utf-8',
                '/I'+str(root),str(Path(__file__).with_name('test_rhythm_envelope.cpp')),
                '/Fo:'+str(output/'test_rhythm_envelope.obj'),'/Fe:'+str(output/'test_rhythm_envelope.exe')],
               cwd=output,check=True)
subprocess.run([str(output/'test_rhythm_envelope.exe')],cwd=output,check=True,timeout=10)
print('PASS optional-null-Rhythm source integration guard (not a full renderer test)')
