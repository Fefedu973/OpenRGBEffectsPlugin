"""Generate an original, quiet 16-second 120 BPM PCM fixture. NEVER plays audio.

This optional integration asset is synthetic, not a real-music quality dataset.
Default output is gitignored private storage. Every note is generated here.
"""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave

root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=root/'private/room-rhythm/120bpm-syncopated-16s.wav')
args=parser.parse_args()
rate=48000
duration=16
events=[]
for beat in range(32):
    time=.15+beat*.5
    events.append((time,85,.048 if beat%4==0 else .030,.026,'kick'))
    if beat%2:
        events.append((time,1150,.025,.018,'mid-accent'))
    events.append((time+.25,6100,.015,.006,'offbeat-hat'))
    if beat%4==3:
        events.append((time+.375,1700,.017,.010,'syncopation'))

samples=[0.0]*(rate*duration)
for time,frequency,gain,decay,_ in events:
    start=round(time*rate)
    for offset in range(round(decay*7*rate)):
        if start+offset>=len(samples):
            break
        age=offset/rate
        envelope=min(1.0,age/.0015)*math.exp(-age/decay)
        samples[start+offset]+=gain*envelope*math.sin(2*math.pi*frequency*age)
peak=max(abs(x) for x in samples)
assert peak<.10, 'Keep synthetic playback level deliberately low.'
pcm=array.array('h',(round(max(-1,min(1,value))*32767) for value in samples))
if sys.byteorder!='little':
    pcm.byteswap()
args.output.parent.mkdir(parents=True,exist_ok=True)
with wave.open(str(args.output),'wb') as output:
    output.setnchannels(1)
    output.setsampwidth(2)
    output.setframerate(rate)
    output.writeframes(pcm.tobytes())
metadata=dict(description='Original generated PCM; synthetic timing fixture, not music quality evidence.',
              bpm=120,sample_rate=rate,channels=1,bits=16,duration_seconds=duration,
              peak_linear=peak,rms_linear=math.sqrt(sum(x*x for x in samples)/len(samples)),
              sha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
              events=[dict(seconds=t,frequency_hz=f,gain=g,decay_seconds=d,kind=k)
                      for t,f,g,d,k in events])
args.output.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(path=str(args.output.resolve()),duration_seconds=duration,peak_linear=peak,
                      sha256=metadata['sha256'],played=False)))
