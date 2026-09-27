// SPDX-License-Identifier: GPL-2.0-or-later
// Original native ring reconstruction. Uniforms are supplied by the engine.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 point=vec2(fragCoord.x,iResolution.y-fragCoord.y)/iResolution.xy*vec2(320.0,200.0);
    vec3 rgb=p_color;
    int count=int(clamp(iTapCount,0.0,64.0));
    for(int event=0;event<64;++event)
    {
        if(event>=count) break;
        vec4 tap=iTapEvents[event];
        vec4 meta=iTapMeta[event];
        if(tap.z<0.0 || tap.z>=5.0 || tap.w<=0.0) continue;
        float travel=max(0.0,meta.y);
        float radius=3.0+6.0*travel;
        float distance=abs(length(point-tap.xy)-radius);
        float edge=max(fwidth(distance),0.04);
        float coverage=1.0-smoothstep(p_iWaveWidth*0.5-edge,p_iWaveWidth*0.5+edge,distance);
        float opacity=clamp((5.0-tap.z)/0.75,0.0,1.0)*clamp(tap.w,0.0,1.0);
        vec3 ink;
        if(p_colorMode<0.5) ink=HSVToRGB(vec3(fract(travel/40.0),1.0,1.0));
        else if(p_colorMode<1.5) ink=p_frontColor;
        else ink=HSVToRGB(vec3(fract(meta.x),1.0,1.0));
        rgb=mix(rgb,ink,coverage*opacity);
    }
    fragColor=vec4(clamp(rgb,0.0,1.0),1.0);
}
