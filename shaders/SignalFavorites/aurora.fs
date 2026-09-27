// SPDX-License-Identifier: GPL-2.0-or-later
// Original GPU curtain field with bounded analytic trails.
float auHash(float n)
{
    return fract(sin(n*11.791+2.41)*23197.173);
}

vec3 auHsl(vec3 rgb)
{
    float hi=max(rgb.r,max(rgb.g,rgb.b)), lo=min(rgb.r,min(rgb.g,rgb.b));
    float delta=hi-lo, light=(hi+lo)*0.5, hue=0.0;
    if(delta>0.000001)
    {
        if(hi==rgb.r) hue=(rgb.g-rgb.b)/delta;
        else if(hi==rgb.g) hue=2.0+(rgb.b-rgb.r)/delta;
        else hue=4.0+(rgb.r-rgb.g)/delta;
    }
    return vec3(fract(hue/6.0),delta/max(1.0-abs(2.0*light-1.0),0.000001),light);
}

vec3 auRgb(vec3 hsl)
{
    float c=(1.0-abs(2.0*hsl.z-1.0))*hsl.y;
    float v=hsl.z+c*0.5;
    return HSVToRGB(vec3(fract(hsl.x),c/max(v,0.000001),v));
}

vec4 auGradient(vec3 base, float height)
{
    float segment=clamp(height,0.0,0.999999)*4.0;
    float k=floor(segment), blend=fract(segment);
    float a0=0.9,a1=0.75;
    if(k>=1.0){a0=0.75;a1=0.7;}
    if(k>=2.0){a0=0.7;a1=0.1;}
    if(k>=3.0){a0=0.1;a1=0.1;}
    vec3 lower=auRgb(base+vec3(k*50.0/360.0,0.0,0.0));
    vec3 upper=auRgb(base+vec3((k+1.0)*50.0/360.0,0.0,0.0));
    return vec4(mix(lower,upper,blend),mix(a0,a1,blend));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 point=vec2(fragCoord.x,iResolution.y-fragCoord.y)/iResolution.xy*vec2(320.0,200.0);
    float hueShift=p_colorCycle>0.5?t_cycleSpeed/300.0:0.0;
    vec3 base=auHsl(p_backColor);base.x+=hueShift;
    vec3 rgb=auRgb(base);
    vec3 front=auHsl(p_frontColor);front.x+=hueShift;
    float density=clamp(p_amount*0.25,0.0,25.0);
    float phase=t_effectSpeed*60.0/38.0;
    for(int index=0;index<25;++index)
    {
        float enabled=clamp(density-float(index),0.0,1.0);
        if(enabled<=0.0) continue;
        float seed=float(index)+17.0;
        float center=mod(phase+480.0*auHash(seed),480.0)-160.0;
        float bottom=80.0+160.0*auHash(seed+37.0)
                    +8.0*sin(t_effectSpeed*0.012+seed)+3.0*sin(t_effectSpeed*0.043+seed*1.7);
        float halfWidth=5.0+7.5*auHash(seed+73.0);
        float across=point.x-center;
        float trail=max(0.25,p_effectSpeed*0.64);
        float horizontal=across < -halfWidth ? exp((across+halfWidth)/trail)
                         : 1.0-smoothstep(halfWidth-0.5,halfWidth+0.5,across);
        float vertical=smoothstep(bottom-201.0,bottom-200.0,point.y)
                       *(1.0-smoothstep(bottom,bottom+1.0,point.y));
        vec4 ink=auGradient(front,(bottom-point.y)/200.0);
        float opacity=clamp(enabled*horizontal*vertical*ink.a,0.0,1.0);
        rgb=mix(rgb,ink.rgb,opacity);
    }
    fragColor=vec4(clamp(rgb,0.0,1.0),1.0);
}
