// SPDX-License-Identifier: GPL-2.0-or-later
// Original procedural reference; no inference model or image-generation claim.
// iChannel0: linear RGB grid; iChannel1: 3x128 numeric event rows, both RGBA32F.
// The owner supplies mainImage and calls iaVideoImage for the video modes.
uniform vec4 iaScreen; // reference rectangle in the logical 320x200 canvas
uniform float iaScreenHeight;
uniform float iaAge; // now - snapshot.source_time, never a large absolute clock
uniform float iaSourceAge; // now - last verified source liveness heartbeat
uniform float iaPersistence;
uniform float iaStrength;
uniform float iaPredictive;
uniform float iaValid;
uniform float iaSuppress;
uniform float iaEventCount;

vec3 iaLinearToSrgb(vec3 c)
{
    c=clamp(c,0.0,1.0);
    return mix(12.92*c,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));
}
vec3 iaGridCell(ivec2 cell,ivec2 size)
{
    return texelFetch(iChannel0,clamp(cell,ivec2(0),size-ivec2(1)),0).rgb;
}
vec3 iaBase(vec2 world)
{
    ivec2 size=textureSize(iChannel0,0);
    vec2 position=clamp(world/vec2(1.0,iaScreenHeight),0.0,1.0)*vec2(size)-0.5;
    ivec2 index=ivec2(floor(position));vec2 f=fract(position);
    return mix(mix(iaGridCell(index,size),iaGridCell(index+ivec2(1,0),size),f.x),
               mix(iaGridCell(index+ivec2(0,1),size),iaGridCell(index+ivec2(1,1),size),f.x),f.y);
}
float iaOutside(vec2 p)
{
    return length(p-clamp(p,vec2(0),vec2(1,iaScreenHeight)));
}
vec3 iaVideoLinear(vec2 world)
{
    // Dynamic input expiry is evaluated by ShaderPass on the GL thread, even
    // when the producer/StepEffect stops and its uniforms remain frozen.
    if(iScreenAvailable<0.5 || any(notEqual(textureSize(iChannel1,0),ivec2(3,128))))return vec3(0);
    if(iaValid<0.5 || !(iaAge>=0.0) || !(iaSourceAge>=0.0 && iaSourceAge<=0.75) || iaScreenHeight<=0.0)return vec3(0);
    vec3 color=iaBase(world);
    float outside=iaOutside(world);
    if(iaPredictive<0.5 || iaStrength<=0.0 || iaSuppress>0.5 || outside==0.0 || outside>1.25)return color;
    int count=int(clamp(iaEventCount,0.0,128.0));
    for(int i=0;i<128;++i)
    {
        if(i>=count)break;
        vec4 motion=texelFetch(iChannel1,ivec2(0,i),0);
        vec4 appearance=texelFetch(iChannel1,ivec2(1,i),0);
        vec4 lifetime=texelFetch(iChannel1,ivec2(2,i),0);
        float age=lifetime.x+iaAge, born=lifetime.y+iaAge;
        if(age>3.0*iaPersistence || born>6.0*iaPersistence)continue;
        vec2 center=motion.xy+motion.zw*iaAge;
        float distanceOutside=iaOutside(center);
        if(distanceOutside>1.25)continue;
        float sigma2=lifetime.z*lifetime.z+0.002*max(0.0,age);
        vec2 delta=world-center;
        float gain=iaStrength*appearance.w*exp(-age/iaPersistence-dot(delta,delta)/(2.0*sigma2))*exp(-distanceOutside/1.25);
        color+=appearance.rgb*gain;
    }
    return clamp(color,0.0,1.0);
}
void iaVideoImage(out vec4 color,in vec2 fragCoord)
{
    if(iaScreen.z<=0.0||iaScreen.w<=0.0){color=vec4(0,0,0,1);return;}
    vec2 canvas=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y)*vec2(320,200);
    vec2 world=(canvas-iaScreen.xy)/iaScreen.zw*vec2(1,iaScreenHeight);
    color=vec4(iaLinearToSrgb(iaVideoLinear(world)),1);
}
