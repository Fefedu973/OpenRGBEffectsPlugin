// SPDX-License-Identifier: GPL-2.0-or-later
// Original spatial audio preset. Three columns: low, middle, high FFT ranges.
// Coordinates are top-left normalized, matching the Visual Map layout.
// iAudio is the Effects filtered spectrum, not independent Hz-labelled bins.
const float IDLE_LEVEL = 0.025;
const float AUDIO_GATE = 0.015;

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
        value = IDLE_LEVEL + energy * (0.25 + 0.70*halo) * texture;
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
