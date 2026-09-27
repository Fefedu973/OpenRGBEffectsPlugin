// SPDX-License-Identifier: GPL-2.0-or-later
// Continuous rising flame field, without flashing whole-canvas frames.
#define RISE_SPEED 0.35
float roomHash(vec2 p) { return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453); }
float roomNoise(vec2 p)
{
    vec2 cell = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
    return mix(mix(roomHash(cell),roomHash(cell+vec2(1,0)),f.x),
               mix(roomHash(cell+vec2(0,1)),roomHash(cell+vec2(1,1)),f.x),f.y);
}
void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = pixel / iResolution.xy;
    vec2 flow = vec2(uv.x*6.0, uv.y*4.0-iTime*RISE_SPEED);
    float n = 0.65*roomNoise(flow)+0.25*roomNoise(flow*2.0)+0.1*roomNoise(flow*4.0);
    float heat = clamp((1.0-uv.y)*0.85+n*0.8-0.35,0.0,1.0);
    color = vec4(heat, pow(heat,2.7)*0.62, pow(heat,6.0)*0.08, 1.0);
}
