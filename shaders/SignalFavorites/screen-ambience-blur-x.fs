// SPDX-License-Identifier: GPL-2.0-or-later
// Full-resolution FBOs use OpenGL bottom-left UVs; source data uses top-left.
vec4 screenRead2(vec2 uv)
{
    if(any(lessThan(uv,vec2(0)))||any(greaterThanEqual(uv,vec2(1))))return vec4(0);
    ivec2 size=textureSize(iChannel2,0);vec2 p=uv*vec2(size)-0.5;ivec2 a=ivec2(floor(p));vec2 f=fract(p);ivec2 hi=size-1;
    return mix(mix(texelFetch(iChannel2,clamp(a,ivec2(0),hi),0),texelFetch(iChannel2,clamp(a+ivec2(1,0),ivec2(0),hi),0),f.x),mix(texelFetch(iChannel2,clamp(a+ivec2(0,1),ivec2(0),hi),0),texelFetch(iChannel2,clamp(a+ivec2(1),ivec2(0),hi),0),f.x),f.y);
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 uv=fragCoord/iResolution.xy;
    if(p_blur_amount<=0.0){fragColor=screenRead2(uv);return;}
    float sigma=p_blur_amount,total=0.0;vec4 result=vec4(0);
    for(int k=-60;k<=60;++k){float d=float(k);if(abs(d)>ceil(3.0*sigma))continue;float weight=exp(-0.5*d*d/(sigma*sigma));total+=weight;result+=weight*screenRead2(uv+vec2(d/320.0,0));}
    fragColor=result/total;
}
