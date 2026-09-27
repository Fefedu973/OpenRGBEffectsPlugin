// SPDX-License-Identifier: GPL-2.0-or-later
// Up to ten native-state random color layers, composed in their birth order.
vec3 shiftHsl(float hue,float saturation,float lightness)
{
    vec3 pure=HSVToRGB(vec3(hue,1.0,1.0));
    float chroma=(1.0-abs(2.0*lightness-1.0))*saturation;
    return (pure-0.5)*chroma+lightness;
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec3 color=vec3(0.05);
    for(int layer=0;layer<10;++layer)
    {
        if(float(layer)>=bShiftCount)break;
        vec4 state=bShift[layer];
        vec3 next=shiftHsl(state.x,p_saturation/100.0,p_lightness/100.0);
        color=mix(color,next,clamp(state.y,0.0,1.0));
    }
    fragColor=vec4(color,1.0);
}
