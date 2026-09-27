// SPDX-License-Identifier: GPL-2.0-or-later
#define FLOAT_SPEED 0.2
void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = (pixel-0.5*iResolution.xy)/iResolution.y;
    vec3 rgb = vec3(0.008);
    float t = iTime*FLOAT_SPEED;
    for(int i=0;i<8;++i)
    {
        float k = float(i);
        vec2 center = vec2(0.65*sin(t*(0.7+k*0.07)+k*2.3),0.4*sin(t*(0.9+k*0.05)+k*1.7));
        float d = length(uv-center), radius = 0.075+0.018*mod(k,3.0);
        float edge = exp(-abs(d-radius)*65.0);
        rgb += HSVToRGB(vec3(fract(k*0.137+t*0.035),0.75,edge*0.55));
    }
    color = vec4(clamp(rgb,0.0,1.0),1.0);
}
