// SPDX-License-Identifier: GPL-2.0-or-later
// Original device-oriented music canvas. Coordinates use a 320x200 design grid.
// iRhythm: BPM, phase, confidence, pulse envelope from continuous audio.
// iOnset: low/mid/high spectral flux envelopes and transient accent.
// ---- User controls --------------------------------------------------------
const int COLOR_STYLE = 2; // 0 static, 1 moving hue wave, 2 color on musical pulse.
const vec3 STATIC_COLOR = vec3(0.02,0.90,1.0);
const float BACKGROUND_LEVEL = 0.008;
const float VOLUME_GAIN = 0.85;
const float SPECTRUM_GAIN = 1.80;
const bool SCALE_SPECTRUM_BY_VOLUME = true;
const int VISIBLE_BINS = 64; // 8..64 real input magnitudes, not Hz labels.
const float BAR_FILL = 0.55; // Thin bars; set 1.0 for a continuous filled shape.
const float LINE_HEIGHT = 15.0;
const float HUE_SPEED = 0.018;
// -------------------------------------------------------------------------

vec3 palette(vec2 p)
{
    if(COLOR_STYLE == 0) return STATIC_COLOR;
    float hue = COLOR_STYLE == 2 ? iMusic.z : fract(iTime*HUE_SPEED+abs(p.x-184.0)/272.0);
    return HSVToRGB(vec3(hue,0.94,1.0));
}

float magnitude(float bin)
{
    int index = int(clamp(bin,0.0,float(VISIBLE_BINS-1)));
    float value = clamp(iAudio[index*4]*SPECTRUM_GAIN,0.0,1.0);
    return value < 0.012 ? 0.0 : pow(value,0.65);
}

void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 p = vec2(pixel.x/iResolution.x,1.0-pixel.y/iResolution.y)*vec2(320.0,200.0);
    float volume = clamp(iMusic.x*VOLUME_GAIN,0.0,1.0);
    float bass = clamp(iMusic.y,0.0,1.0);
    float pulse = max(iRhythm.w,0.35*iOnset.w);
    vec3 ink = palette(p);
    float brightness = BACKGROUND_LEVEL;
    if(p.x < 48.0)
    {
        // Broadband VU fills upward. A continuous field samples cleanly on
        // strips and RAM; this is not divided into display-like segments.
        float fill = 1.0-smoothstep(volume-0.004,volume+0.004,(200.0-p.y)/200.0);
        brightness = max(brightness,step(0.001,volume)*fill);
    }
    else if(p.y < 30.0)
    {
        if(p.x < 180.0)
            brightness = max(brightness,step(0.001,bass)*(1.0-smoothstep(bass-0.004,bass+0.004,(p.x-48.0)/132.0)));
        else if(p.x < 220.0) brightness = max(brightness,pulse);
        else if(p.x < 260.0) brightness = max(brightness,max(volume*0.55,iOnset.y));
        else if(p.x < 290.0)
        {
            // Circular sector starts at the bottom and sweeps clockwise.
            vec2 q = p-vec2(275.0,15.0);
            float sweep = fract(atan(q.x,-q.y)/6.2831853+0.5);
            float fill = iRhythm.z>0.35 ? 1.0-iRhythm.y : volume;
            brightness = max(brightness,step(0.001,volume)*(1.0-smoothstep(fill-0.003,fill+0.003,sweep)));
        }
        else brightness = max(brightness,p.y >= 10.0 && p.y < 20.0 ? max(pulse,iOnset.z) : 0.025);
    }
    else
    {
        // Low frequencies start at the center; the spectrum is mirrored in
        // both axes. Color is shared across the whole room on each onset.
        float position = clamp(abs(p.x-184.0)/136.0,0.0,0.99999);
        float slot = position*float(VISIBLE_BINS);
        float level = magnitude(floor(slot));
        if(SCALE_SPECTRUM_BY_VOLUME) level *= volume;
        if(p.y >= 200.0-LINE_HEIGHT)
            brightness = max(brightness,clamp(level*1.5,0.0,1.0));
        else
        {
            float center = (30.0+200.0-LINE_HEIGHT)*0.5;
            float extent = (200.0-LINE_HEIGHT-30.0)*0.5;
            float height = abs(p.y-center)/extent;
            float width = 1.0-smoothstep(BAR_FILL*0.5,BAR_FILL*0.5+0.08,abs(fract(slot)-0.5));
            float shape = 1.0-smoothstep(level-0.003,level+0.006,height);
            brightness = max(brightness,step(0.001,level)*width*shape);
        }
    }
    // A restrained room-wide pulse links spectrum, VU and physical zones without
    // replacing their independent detail with a full-screen white flash.
    brightness=max(brightness,0.15*pulse*volume);
    color = vec4(ink*brightness,1.0);
}
