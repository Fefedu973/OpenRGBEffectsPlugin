// SPDX-License-Identifier: GPL-2.0-or-later
// Averages H, S and L independently, including the reference's non-circular
// hue average. Numeric texture alpha is the native tap/recovery gain.
vec3 averageHsl(vec3 c)
{
    vec3 k=clamp(abs(mod(c.x*6.0+vec3(0,4,2),6.0)-3.0)-1.0,0.0,1.0);
    return c.z+c.y*(1.0-abs(2.0*c.z-1.0))*(k-0.5);
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    if(iScreenAvailable<0.5){fragColor=vec4(0,0,0,1);return;}
    vec4 state=texelFetch(iChannel1,ivec2(0),0);
    fragColor=vec4(clamp(averageHsl(state.xyz),0.0,1.0)*state.w,1);
}
