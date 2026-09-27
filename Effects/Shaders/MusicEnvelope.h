// SPDX-License-Identifier: GPL-2.0-or-later
// Original spectrum-derived envelopes and onset color state. No BPM estimate.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

class MusicEnvelope
{
public:
    void Reset() { *this = MusicEnvelope(); }

    // The Effects DSP provides 64 magnitudes repeated in groups of four.
    // Time is monotonic wall time, independent of shader animation speed.
    std::array<float, 4> Update(const float* audio, double seconds)
    {
        if(!std::isfinite(seconds)) return values;
        if(initialized && (seconds < previous_time || seconds-previous_time > 1.0)) Reset();
        double dt = initialized ? seconds-previous_time : 1.0/60.0;
        previous_time = seconds;
        initialized = true;
        float power = 0.0f, bass = 0.0f;
        if(audio)
        {
            for(unsigned bin=0; bin<64; ++bin)
            {
                float value = std::isfinite(audio[bin*4]) ? std::clamp(audio[bin*4],0.0f,1.0f) : 0.0f;
                power += value*value;
                if(bin >= 1 && bin <= 3) bass += value/3.0f;
            }
        }
        float volume = std::clamp(std::sqrt(power/64.0f)*2.6f,0.0f,1.0f);
        bass = std::clamp(bass*1.8f,0.0f,1.0f);
        if(volume < 0.012f) volume = bass = 0.0f;
        // Attack is immediate; release is time based and remains stable at
        // different frame rates. A long pause resets stale envelopes above.
        values[0] = std::max(volume,values[0]*float(std::exp(-dt/0.16)));
        values[1] = std::max(bass,values[1]*float(std::exp(-dt/0.09)));
        values[3] *= float(std::exp(-dt/0.12));
        // Detect a rising bass transient above recent energy, not a clock.
        // Continuous tones cannot keep changing the hue. The 140 ms guard
        // prevents rapid repeated events, without assuming a music tempo.
        bool onset = bass > std::max(0.10f,baseline*1.65f+0.035f) &&
                     bass-previous_bass > 0.018f && seconds-last_onset >= 0.14;
        if(onset)
        {
            random_state = random_state*1664525u+1013904223u;
            float step = 0.20f+0.55f*float((random_state>>8)&0xffffu)/65535.0f;
            values[2] = std::fmod(values[2]+step,1.0f);
            values[3] = 1.0f;
            last_onset = seconds;
        }
        baseline += (bass-baseline)*float(1.0-std::exp(-dt/0.32));
        previous_bass = bass;
        // Missing audio is a disconnected source, not a decaying old beat.
        if(!audio) { values[0]=values[1]=values[3]=0.0f; baseline=previous_bass=0.0f; }
        return values;
    }

private:
    std::array<float,4> values{0.0f,0.0f,0.54f,0.0f};
    double previous_time = 0.0, last_onset = -1000.0;
    float baseline = 0.0f, previous_bass = 0.0f;
    bool initialized = false;
    std::uint32_t random_state = 0x524f4f4du;
};
