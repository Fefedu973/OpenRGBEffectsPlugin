// SPDX-License-Identifier: GPL-2.0-or-later
vec4 screenRead3(vec2 uv)
{
    if(any(lessThan(uv,vec2(0)))||any(greaterThanEqual(uv,vec2(1))))return vec4(0);
    ivec2 size=textureSize(iChannel3,0);vec2 p=uv*vec2(size)-0.5;ivec2 a=ivec2(floor(p));vec2 f=fract(p);ivec2 hi=size-1;
    return mix(mix(texelFetch(iChannel3,clamp(a,ivec2(0),hi),0),texelFetch(iChannel3,clamp(a+ivec2(1,0),ivec2(0),hi),0),f.x),mix(texelFetch(iChannel3,clamp(a+ivec2(0,1),ivec2(0),hi),0),texelFetch(iChannel3,clamp(a+ivec2(1),ivec2(0),hi),0),f.x),f.y);
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    if(iScreenAvailable<0.5){fragColor=vec4(0,0,0,1);return;}
    vec2 uv=fragCoord/iResolution.xy;
    if(p_blur_amount<=0.0){fragColor=vec4(screenRead3(uv).rgb,1);return;}
    float sigma=p_blur_amount,total=0.0;vec3 result=vec3(0);
    for(int k=-60;k<=60;++k){float d=float(k);if(abs(d)>ceil(3.0*sigma))continue;float weight=exp(-0.5*d*d/(sigma*sigma));total+=weight;result+=weight*screenRead3(uv+vec2(0,d/200.0)).rgb;}
    fragColor=vec4(result/total,1);
}
