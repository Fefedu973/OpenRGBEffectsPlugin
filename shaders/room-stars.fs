// SPDX-License-Identifier: GPL-2.0-or-later
#define TWINKLE_SPEED 0.55
float starHash(vec2 p) { return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = pixel/iResolution.xy;
    vec2 p = uv*vec2(16,10), cell = floor(p), f = fract(p)-0.5;
    float seed = starHash(cell);
    float glow = exp(-dot(f,f)*24.0);
    float twinkle = pow(0.5+0.5*sin(iTime*TWINKLE_SPEED*(0.6+seed)+seed*27.0),3.0);
    color = vec4(vec3(0.008,0.01,0.025)+HSVToRGB(vec3(seed,0.25,glow*twinkle*0.9)),1.0);
}
