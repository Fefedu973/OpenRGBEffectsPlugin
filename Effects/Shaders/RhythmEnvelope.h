// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Audio/RhythmTracker.h"
#include <array>
#include <algorithm>
#include <cmath>

// Convert audio-clock observations into continuous display envelopes. Predicted
// beats are emitted only while the audio tracker is locked and data is fresh.
class RhythmEnvelope
{
public:
    struct Output { std::array<float,4> rhythm{}, transients{}; float hue=0.54f; };
    void Reset() { *this=RhythmEnvelope(); }
    Output Update(const room_audio::RhythmSnapshot& input,double now)
    {
        if(!std::isfinite(now)) return {};
        if(!initialized || input.generation!=generation || now<previous_time || now-previous_time>1)
        { Reset(); initialized=true; generation=input.generation; previous_time=now; last_onset=input.onset_sequence; }
        const double dt=std::clamp(now-previous_time,0.0,0.25); previous_time=now;
        const double age=now-input.audio_time;
        if(input.silent || !input.sequence || !std::isfinite(age) || age>0.15 || age< -0.1)
        { beat=transient=0; bands.fill(0); phase_valid=false; return {{},{},hue}; }
        beat*=float(std::exp(-dt/0.10)); transient*=float(std::exp(-dt/0.075));
        for(unsigned i=0;i<3;++i) bands[i]=std::max(bands[i]*float(std::exp(-dt/0.085)),std::clamp(input.band_flux[i]*0.25f,0.f,1.f));
        const double onset_age=now-input.last_onset_time;
        const bool onset=input.onset_sequence!=last_onset && onset_age>=0 && onset_age<=0.15;
        last_onset=input.onset_sequence;
        if(onset) transient=1;
        const bool locked=input.locked && input.confidence>=0.35f && input.bpm>=60 && input.bpm<=180;
        float phase=0;
        bool pulse=false;
        if(locked)
        {
            phase=float(std::fmod(input.phase+std::max(0.0,age)*input.bpm/60.0,1.0));
            // Do not create a beat merely because confidence has just appeared.
            pulse=phase_valid && phase<previous_phase-0.5f;
            previous_phase=phase; phase_valid=true;
        }
        else { phase_valid=false; pulse=onset; }
        if(pulse)
        {
            beat=1;
            // A repeatable palette progression on the pulse, not random colors
            // selected for every change in low-frequency energy.
            hue=std::fmod(hue+0.1375f,1.f);
        }
        return {{locked?input.bpm:0,phase,locked?input.confidence:0,beat},
                {bands[0],bands[1],bands[2],transient},hue};
    }
private:
    bool initialized=false,phase_valid=false;
    std::uint64_t generation=0,last_onset=0;
    double previous_time=0;
    float previous_phase=0,beat=0,transient=0,hue=0.54f;
    std::array<float,3> bands{};
};
