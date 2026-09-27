// SPDX-License-Identifier: GPL-2.0-or-later
uniform vec4 prState;
// Original 12-direction gradient noise, seeded deterministically. The random
// realization differs from the browser's randomly seeded permutation table.
uint plHash(ivec3 p){uint h=uint(p.x)*0x8da6b343u^uint(p.y)*0xd8163841u^uint(p.z)*0xcb1ab31fu^0x1323u;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);}
float plGrad(ivec3 cell,vec3 delta)
{
    int h=int(plHash(cell)%12u);float a=h<8?delta.x:delta.y;float b=h<4?delta.y:delta.z;
    return ((h%4)<2?a:-a)+((h%2)==0?b:-b);
}
float plNoise(vec3 p)
{
    ivec3 cell=ivec3(floor(p));vec3 f=fract(p),u=f*f*f*(f*(f*6.0-15.0)+10.0);
    float a=mix(plGrad(cell,f),plGrad(cell+ivec3(1,0,0),f-vec3(1,0,0)),u.x);
    float b=mix(plGrad(cell+ivec3(0,1,0),f-vec3(0,1,0)),plGrad(cell+ivec3(1,1,0),f-vec3(1,1,0)),u.x);
    float c=mix(plGrad(cell+ivec3(0,0,1),f-vec3(0,0,1)),plGrad(cell+ivec3(1,0,1),f-vec3(1,0,1)),u.x);
    float d=mix(plGrad(cell+ivec3(0,1,1),f-vec3(0,1,1)),plGrad(cell+ivec3(1,1,1),f-vec3(1,1,1)),u.x);
    return mix(mix(a,b,u.y),mix(c,d,u.y),u.z);
}
vec3 plHsl(vec3 rgb)
{
    float hi=max(rgb.r,max(rgb.g,rgb.b)),lo=min(rgb.r,min(rgb.g,rgb.b)),delta=hi-lo;
    float light=(hi+lo)*.5,hue=0.0,saturation=0.0;
    if(delta>0.0){if(hi==rgb.r)hue=mod((rgb.g-rgb.b)/delta,6.0);else if(hi==rgb.g)hue=(rgb.b-rgb.r)/delta+2.0;else hue=(rgb.r-rgb.g)/delta+4.0;saturation=delta/(1.0-abs(2.0*light-1.0));}
    return vec3(mod(floor(hue*60.0+.5),360.0),floor(saturation*1000.0+.5)/1000.0,floor(light*1000.0+.5)/1000.0);
}
vec3 plRgb(vec3 hsl)
{
    vec3 hue=clamp(abs(fract(hsl.x/360.0+vec3(0,2.0/3.0,1.0/3.0))*6.0-3.0)-1.0,0.0,1.0);
    return hsl.z+(hue-.5)*(1.0-abs(2.0*hsl.z-1.0))*hsl.y;
}
vec3 plStop(int i)
{
    if(i==0||i==5||i==6)return vec3(87,0,163)/255.0;
    if(i==1||i==7)return vec3(122,0,114)/255.0;
    if(i==2||i==8)return vec3(0,79,24)/255.0;
    if(i==3||i==9)return vec3(0,62,107)/255.0;
    return vec3(0,8,92)/255.0;
}
void mainImage(out vec4 color,in vec2 position)
{
    vec2 point=vec2(position.x/iResolution.x,1.0-position.y/iResolution.y)*vec2(320,200);
    float cellSize=p_dotSize+5.0,pitch=cellSize+p_gap;vec2 local=mod(point,pitch),origin=point-local;
    vec3 background=p_bgColor;
    if(p_bgcolorMode>.5){vec3 hsl=plHsl(p_bgColor);hsl.x=prState.z*p_colorCycleSpeed*5.0+160.0;background=plRgb(hsl);}
    if(local.x>=cellSize||local.y>=cellSize){color=vec4(background,1);return;}
    float yn=20.0*plNoise(vec3(origin.y/200.0,origin.x/200.0,prState.x));
    float alpha=p_shape<.5?clamp(yn,0.0,1.0):(p_shape<1.5?1.0:(yn>0.0&&yn<=.999?1.0:0.0));
    vec3 ink;
    if(p_colorMode<.5){vec3 hsl=plHsl(p_pixelColor);hsl.x+=p_hueRange*(.8+.2*yn);ink=plRgb(hsl);}
    else if(p_colorMode<1.5)ink=plRgb(vec3(prState.x*p_colorCycleSpeed*5.0,1,.5));
    else{float t=clamp((point.x+320.0-prState.y)/640.0,0.0,1.0)*10.0;int i=min(9,int(floor(t)));ink=mix(plStop(i),plStop(i+1),t-float(i));}
    color=vec4(mix(background,ink,alpha),1.0);
}
