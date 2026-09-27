// SPDX-License-Identifier: GPL-2.0-or-later
// Original spatial audio preset. Three columns: low, middle, high FFT ranges.
// Coordinates are top-left normalized, matching the Visual Map layout.
// iAudio is the Effects filtered spectrum: 64 magnitudes repeated four times.
// Tune only this block. SPECTRUM_BARS is 64 (detailed) or 32 (pair averages).
const float IDLE_LEVEL = 0.025;
const float AUDIO_GATE = 0.015;
const int SPECTRUM_BARS = 64;
const float SPECTRUM_GAIN = 0.62;
const float CURVE_GAIN = 0.22;
const float CAP_GAIN = 0.18;
const float HALO_GAIN = 0.55;
const float SPECTRUM_CENTER = 0.58; // Upper-half Y; global Y=58 on 320x200.
const float SPECTRUM_REACH = 0.37;
// End controls. Caps follow the current filtered spectrum, not peak-hold/BPM.

float spectrumCount(int band)
{
    float count = band == 0 ? 4.0 : (band == 1 ? 20.0 : 40.0);
    return SPECTRUM_BARS == 32 ? count * 0.5 : count;
}

float spectrumBar(int band, float bar)
{
    int first = band == 0 ? 0 : (band == 1 ? 4 : 24);
    int offset = int(clamp(bar, 0.0, spectrumCount(band)-1.0));
    int bin = first + (SPECTRUM_BARS == 32 ? offset*2 : offset);
    float magnitude = clamp(iAudio[bin*4], 0.0, 1.0);
    if(SPECTRUM_BARS == 32)
        magnitude = 0.5 * (magnitude + clamp(iAudio[(bin+1)*4], 0.0, 1.0));
    float gain = band == 0 ? 1.15 : (band == 1 ? 1.50 : 2.10);
    return smoothstep(AUDIO_GATE, 0.85, magnitude * gain);
}

float spectrumLayers(int band, float x, float y)
{
    float slot = clamp(x, 0.0, 0.99999) * spectrumCount(band);
    float bar = floor(slot);
    float amplitude = spectrumBar(band, bar);
    float distanceFromCenter = abs(y-SPECTRUM_CENTER);
    float edge = SPECTRUM_REACH*amplitude;
    float width = 1.0-smoothstep(0.34, 0.46, abs(fract(slot)-0.5));
    float active = step(0.00001, amplitude);
    float fill = 1.0-smoothstep(edge-0.004, edge+0.009, distanceFromCenter);
    float cap = 1.0-smoothstep(0.003, 0.012, abs(distanceFromCenter-edge));
    // Linear interpolation between actual adjacent magnitudes. No extra bins
    // are fabricated; the curve is simply another view of the same spectrum.
    float between = slot-0.5;
    float curveAmplitude = mix(spectrumBar(band, floor(between)),
                               spectrumBar(band, floor(between)+1.0), fract(between));
    float curve = 1.0-smoothstep(0.004, 0.014,
                                abs(distanceFromCenter-SPECTRUM_REACH*curveAmplitude));
    return active*width*(SPECTRUM_GAIN*fill*(0.35+0.65*amplitude)+CAP_GAIN*cap)
         + step(0.00001, curveAmplitude)*CURVE_GAIN*curve;
}

float bandEnergy(int band)
{
    float sum = 0.0;
    if(band == 0)
    {
        for(int i=0; i<16; i+=4) sum += clamp(iAudio[i], 0.0, 1.0);
        return smoothstep(AUDIO_GATE, 0.55, sum * 0.25 * 1.15);
    }
    if(band == 1)
    {
        for(int i=16; i<96; i+=4) sum += clamp(iAudio[i], 0.0, 1.0);
        return smoothstep(AUDIO_GATE, 0.55, sum * 0.05 * 1.50);
    }
    for(int i=96; i<256; i+=4) sum += clamp(iAudio[i], 0.0, 1.0);
    return smoothstep(AUDIO_GATE, 0.55, sum * 0.025 * 2.10);
}

void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = vec2(pixel.x / iResolution.x, 1.0 - pixel.y / iResolution.y);
    int band = int(min(floor(uv.x * 3.0), 2.0));
    float x = fract(uv.x * 3.0);
    float energy = bandEnergy(band);
    float hue = band == 0 ? 0.96 : (band == 1 ? 0.52 : 0.12);
    hue = fract(hue + 0.045 * sin(iTime * 0.32 + uv.y * 3.0) + 0.045 * x);
    float value;
    if(uv.y < 0.5)
    {
        // Surface/keyboard half: audio-driven radius and brightness, with a
        // moving spatial texture. No time-only strobe or beat/BPM claim.
        vec2 q = vec2((x-0.5)*1.25, (uv.y-0.25)*2.0);
        float radius = length(q);
        float halo = 1.0 - smoothstep(0.15 + 0.35*energy, 0.72, radius);
        float texture = 0.82 + 0.18*sin(x*9.0 + uv.y*13.0 - iTime*1.4);
        value = IDLE_LEVEL + HALO_GAIN*energy*(0.25+0.70*halo)*texture
              + spectrumLayers(band, x, uv.y*2.0);
    }
    else
    {
        // Strip half: meter grows from the bottom toward the middle of canvas.
        float level = (1.0-uv.y)*2.0;
        float fill = 1.0-smoothstep(energy-0.025, energy+0.025, level);
        float lit = step(AUDIO_GATE, energy);
        float segments = 0.70 + 0.30*smoothstep(0.05, 0.25, fract(level*18.0));
        value = IDLE_LEVEL + lit * fill * segments * (0.35 + 0.60*energy);
        hue = fract(hue + level*0.11);
    }
    color = vec4(HSVToRGB(vec3(hue, 0.90, clamp(value, 0.0, 0.96))), 1.0);
}
