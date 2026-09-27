// SPDX-License-Identifier: GPL-2.0-or-later
#define WAVE_SPEED 0.25
void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = (pixel-0.5*iResolution.xy)/iResolution.y;
    float wave = length(uv)*3.5-iTime*WAVE_SPEED;
    float intensity = 0.12+0.7*(0.5+0.5*sin(wave*6.2831853));
    color = vec4(HSVToRGB(vec3(fract(wave*0.12+iTime*0.015),0.87,intensity)),1.0);
}
