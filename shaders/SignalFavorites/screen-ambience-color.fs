// SPDX-License-Identifier: GPL-2.0-or-later
vec3 ambienceHsl(vec3 c)
{
    c.yz=clamp(c.yz,0.0,1.0);
    vec3 k=clamp(abs(mod(c.x*6.0+vec3(0,4,2),6.0)-3.0)-1.0,0.0,1.0);
    return c.z+c.y*(1.0-abs(2.0*c.z-1.0))*(k-0.5);
}
vec3 ambienceHd(vec2 uv)
{
    vec2 p=uv*vec2(160,100)-0.5;ivec2 a=ivec2(floor(p));vec2 f=fract(p);
    vec3 c00=texelFetch(iChannel1,clamp(a,ivec2(0),ivec2(159,99)),0).rgb;
    vec3 c10=texelFetch(iChannel1,clamp(a+ivec2(1,0),ivec2(0),ivec2(159,99)),0).rgb;
    vec3 c01=texelFetch(iChannel1,clamp(a+ivec2(0,1),ivec2(0),ivec2(159,99)),0).rgb;
    vec3 c11=texelFetch(iChannel1,clamp(a+ivec2(1),ivec2(0),ivec2(159,99)),0).rgb;
    return mix(mix(c00,c10,f.x),mix(c01,c11,f.x),f.y);
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    if(iScreenAvailable<0.5){fragColor=vec4(0,0,0,1);return;}
    vec2 uv=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y);
    vec3 color;
    if(bScreenHD>0.5)color=ambienceHd(uv);
    else
    {
        vec3 hsl=texelFetch(iChannel1,clamp(ivec2(floor(uv*vec2(28,20))),ivec2(0),ivec2(27,19)),0).rgb;
        if(p_picture_mode>0.5&&p_picture_mode<1.5)hsl.y-=0.1;
        else if(p_picture_mode>1.5&&p_picture_mode<2.5)hsl.y=0.0;
        else if(p_picture_mode>2.5&&p_picture_mode<3.5)hsl.y+=0.1;
        else if(p_picture_mode>3.5&&p_picture_mode<4.5)hsl.x=bScreenDominantHue;
        color=ambienceHsl(hsl);
    }
    // CSS hue-rotate is a linear RGB matrix, not an HSL hue offset.
    float angle=radians(p_boost),c=cos(angle),s=sin(angle);
    color=clamp(vec3(
        dot(color,vec3(.213+.787*c-.213*s,.715-.715*c-.715*s,.072-.072*c+.928*s)),
        dot(color,vec3(.213-.213*c+.143*s,.715+.285*c+.140*s,.072-.072*c-.283*s)),
        dot(color,vec3(.213-.213*c-.787*s,.715-.715*c+.715*s,.072+.928*c+.072*s))),0.0,1.0);
    color=clamp(color*(1.0+p_brightness/100.0),0.0,1.0);
    float saturation=1.0+p_saturation/100.0;
    float luma=dot(color,vec3(.213,.715,.072));
    color=clamp(vec3(luma)+(color-vec3(luma))*saturation,0.0,1.0);
    color=clamp((color-0.5)*(1.0+p_contrast/100.0)+0.5,0.0,1.0);
    fragColor=vec4(color,1);
}
