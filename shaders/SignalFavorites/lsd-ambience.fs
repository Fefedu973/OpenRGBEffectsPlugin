// SPDX-License-Identifier: GPL-2.0-or-later
// Ordered circles at the reference's asymmetric cell centers, not a warped
// screen texture. Native state supplies draw-before-update radius and HSL.
vec3 lsdHsl(vec3 c)
{
    vec3 k=clamp(abs(mod(c.x*6.0+vec3(0,4,2),6.0)-3.0)-1.0,0.0,1.0);
    return c.z+c.y*(1.0-abs(2.0*c.z-1.0))*(k-0.5);
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    if(iScreenAvailable<0.5){fragColor=vec4(0,0,0,1);return;}
    vec2 p=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y)*vec2(320,200);
    float aa=max(length(vec2(320,200)/iResolution.xy)*0.5,0.25);
    float reach=bScreenMaxRadius+aa;
    ivec2 low=ivec2(max(vec2(0),ceil((p-vec2(5)-reach)/vec2(320.0/28.0,10))));
    ivec2 high=ivec2(min(vec2(27,19),floor((p-vec2(5)+reach)/vec2(320.0/28.0,10))));
    vec3 result=vec3(0);
    for(int y=0;y<20;++y)
    {
        int row=low.y+y;if(row>high.y)break;
        for(int x=0;x<28;++x)
        {
            int column=low.x+x;if(column>high.x)break;
            vec4 state=texelFetch(iChannel1,ivec2(column,row),0);
            vec2 center=vec2(float(column)*(320.0/28.0)+5.0,float(row)*10.0+5.0);
            float alpha=state.w<=0.0?0.0:clamp(0.5+(state.w-length(p-center))/aa,0.0,1.0);
            result=mix(result,clamp(lsdHsl(state.xyz),0.0,1.0),alpha);
        }
    }
    fragColor=vec4(result,1);
}
