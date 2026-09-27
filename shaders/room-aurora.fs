// SPDX-License-Identifier: GPL-2.0-or-later
#define FLOW_SPEED 0.25
void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = pixel / iResolution.xy;
    float t = iTime*FLOW_SPEED;
    float ribbon = 0.5+0.15*sin(uv.x*7.0+t)+0.08*sin(uv.x*17.0-t*1.3);
    float glow = exp(-abs(uv.y-ribbon)*7.0);
    float fold = 0.65+0.35*sin(uv.x*24.0+t*2.0);
    color = vec4(HSVToRGB(vec3(0.40+0.22*sin(uv.x*2.0+t*0.2),0.82,0.04+0.72*glow*fold)),1.0);
}
