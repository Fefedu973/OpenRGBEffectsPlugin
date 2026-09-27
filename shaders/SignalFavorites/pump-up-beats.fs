// SPDX-License-Identifier: GPL-2.0-or-later
// Original native implementation of the audited 320 x 200 multi-region design.
// Audio dynamics are bounded scalar state, rasterization and fading run on GPU.
uniform vec4 pumpLevels;
uniform vec4 pumpState;
uniform float pumpFreq[100];
uniform vec3 pumpScreenColor;

vec3 pumpHue(float h)
{return clamp(abs(fract(h+vec3(0.0,2.0/3.0,1.0/3.0))*6.0-3.0)-1.0,0.0,1.0);}
vec3 pumpHSLHue(vec3 rgb,float hue)
{
    float lo=min(rgb.r,min(rgb.g,rgb.b)),hi=max(rgb.r,max(rgb.g,rgb.b));
    float light=(hi+lo)*0.5;
    return vec3(light)+(pumpHue(hue)-vec3(0.5))*(hi-lo);
}
// The reference palette is a five-segment RGB gradient, not a continuously
// evaluated HSV wheel; preserve the interpolation between those color stops.
vec3 pumpRainbow(float x)
{
    float q=fract(x)*5.0,i=floor(q);
    return mix(pumpHue(i/5.0),pumpHue((i+1.0)/5.0),fract(q));
}
float pumpInside(vec2 p,vec4 r)
{return step(r.x,p.x)*step(r.y,p.y)*(1.0-step(r.x+r.z,p.x))*(1.0-step(r.y+r.w,p.y));}
float pumpProject(vec2 p,vec4 limits)
{vec2 axis=limits.zw-limits.xy;return dot(p-limits.xy,axis)/max(dot(axis,axis),0.00001);}
vec3 pumpBackground()
{
    vec3 c=p_backgroundCol_in;
    if(p_backgroundStyle>0.5 && p_backgroundStyle<1.5)c=pumpHSLHue(c,pumpState.x);
    else if(p_backgroundStyle>1.5)c=pumpHSLHue(c,pumpState.y);
    vec3 flash=vec3(0.0);
    if(p_backgroundMode>0.5 && p_backgroundMode<1.5)flash=vec3(1.0);
    else if(p_backgroundMode<2.5)flash=pumpHue(pumpState.y);
    else if(p_backgroundMode<3.5)flash=p_staticCol1;
    else if(p_backgroundMode<4.5)flash=p_staticCol2;
    else flash=p_backgroundCol_in;
    return p_backgroundMode>0.5?mix(c,flash,pumpState.z):c;
}
// split=0 horizontal two-color, 1 vertical, 2 first, 3 second.
void pumpRect(inout vec3 c,vec2 p,vec4 r,vec4 limits,float brightness,float split,vec3 bg,bool bottom)
{
    if(pumpInside(p,r)<0.5)return;
    float value=clamp(brightness,0.0,1.0);
    if(p_colorStyle<0.5)
    {
        float second=split<0.5?step(r.x+r.z*0.5,p.x):(split<1.5?step(r.y+r.w*0.5,p.y):step(2.5,split));
        c=mix(bg,mix(p_staticCol1,p_staticCol2,second),value);
    }
    else if(p_colorStyle<1.5)c=mix(bg,pumpHue(pumpState.x),value);
    else if(p_colorStyle<3.5)
    {
        c=mix(c,pumpRainbow(pumpProject(p,limits)+4.0*pumpState.x),value);
        // Historical bottom-line block is a second source-over background
        // layer whose alpha is multiplied by the rectangle's global alpha.
        if(bottom)c=mix(c,p_backgroundCol_in,value*value);
    }
    else if(p_colorStyle<4.5)c=mix(bg,pumpHue(pumpState.y),value);
    else c=mix(bg,pumpScreenColor,value);
}
float pumpSmoothHeight(float distance,float bw,float n)
{
    float x0=-0.5,y0=pumpFreq[0],x1=bw,y1=pumpFreq[1];
    if(distance<=x1)return mix(y0,y1,clamp((distance-x0)/(x1-x0),0.0,1.0));
    x0=x1;y0=y1;
    for(int i=1;i<99;++i)
    {
        if(float(i)>=n-1.0)break;
        if(p_displayStyle<3.5 || mod(float(i),3.0)<0.5)
        {
            x1=(float(i)+1.0)*bw;y1=pumpFreq[i+1];
            if(distance<=x1)return mix(y0,y1,(distance-x0)/(x1-x0));
            x0=x1;y0=y1;
        }
    }
    return mix(y0,0.0,clamp((distance-x0)/max(n*bw-x0,0.0001),0.0,1.0));
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 uv=fragCoord/iResolution.xy;
    vec2 p=vec2(uv.x,1.0-uv.y)*vec2(320.0,200.0);
    float top=200.0-p_freqDisplaySize,left=320.0-1.6*p_freqDisplaySize;
    float remaining=210.0-left-top;
    float bottom=p_bottomLineThickness,n=clamp(floor(p_usedFreqSector+0.5),10.0,100.0);
    vec3 bg=pumpBackground();
    vec4 previous=texture2D(iPreviousFrame,uv);
    float fade=p_fadingOut<0.5?1.0:(51.0-p_fadingOut)/255.0;
    vec3 c=mix(previous.a>0.5?previous.rgb:vec3(0.0),bg,fade);
    if(pumpInside(p,vec4(left+remaining,0.0,80.0,top))>0.5 ||
       pumpInside(p,vec4(290.0,0.0,30.0,top))>0.5 ||
       pumpInside(p,vec4(left,200.0-bottom,320.0-left,bottom))>0.5)c=p_backgroundCol_in;

    // A clipped fan sector; the reference fills a square, not an annular ring.
    if(pumpInside(p,vec4(290.0-top,0.0,top,top))>0.5)
    {
        vec2 d=p-vec2(290.0-top*0.5,top*0.5);
        float a=fract((atan(d.y,d.x)-1.57079632679)/6.28318530718);
        bool gradient=p_colorStyle<0.5 || (p_colorStyle>1.5 && p_colorStyle<3.5);
        float angle=gradient?floor(a*50.0)/50.0:a;
        if(angle<pumpLevels.x && (!gradient || pumpLevels.x>=0.02))
        {
            if(gradient)
            {
                float q=angle*0.5+pumpState.x+length(d)/(3.14159265359*top);
                if(p_colorStyle<0.5)c=mix(p_staticCol1,p_staticCol2,1.0-abs(fract(q)*2.0-1.0));
                else c=pumpRainbow(q);
            }
            else c=p_colorStyle>4.5?pumpScreenColor:pumpHue(p_colorStyle<1.5?pumpState.x:pumpState.y);
        }
    }
    vec2 center=vec2((320.0+left)*0.5,(top+200.0-bottom)*0.5);
    vec2 dims=vec2(320.0-left,200.0-top-bottom);
    float bw=dims.x*0.5/n;
    vec4 limitsR=vec4(320.0,0.0,center.x,0.0),limitsL=vec4(left,0.0,center.x,0.0);
    if(p_colorStyle>2.5 && p_colorStyle<3.5)limitsR=limitsL=vec4(0.0,top+bottom,0.0,200.0);
    for(int i=0;i<100;++i)
    {
        if(float(i)>=n)break;
        float v=clamp(pumpFreq[i],0.0,1.0),xR=center.x+bw*(float(i)-1.0),xL=center.x-bw*(float(i)+1.0);
        if(p_displayStyle<2.5)
        {
            float thick=p_displayStyle<0.5?2.0:(p_displayStyle<1.5?1.0:5.0);
            pumpRect(c,p,vec4(xR,center.y-v*dims.y*0.5,bw*thick,v*dims.y),limitsR,1.0,3.0,bg,false);
            pumpRect(c,p,vec4(xL,center.y-v*dims.y*0.5,bw*thick,v*dims.y),limitsL,1.0,2.0,bg,false);
        }
        else if(p_displayStyle>4.5)
        {
            pumpRect(c,p,vec4(xR,center.y-v*dims.y*0.5,bw*1.5,3.0),limitsR,1.0,3.0,bg,false);
            pumpRect(c,p,vec4(xR,center.y+v*dims.y*0.5,bw*1.5,3.0),limitsR,1.0,2.0,bg,false);
            pumpRect(c,p,vec4(xL,center.y-v*dims.y*0.5,bw*1.5,3.0),limitsL,1.0,3.0,bg,false);
            pumpRect(c,p,vec4(xL,center.y+v*dims.y*0.5,bw*1.5,3.0),limitsL,1.0,2.0,bg,false);
        }
        pumpRect(c,p,vec4(xR,200.0-bottom,bw,bottom),limitsR,min(1.0,v*2.0),3.0,bg,true);
        pumpRect(c,p,vec4(xL,200.0-bottom,bw,bottom),limitsL,min(1.0,v*2.0),2.0,bg,true);
    }
    if(p_displayStyle>2.5 && p_displayStyle<4.5 && p.x>=left)
    {
        float distance=abs(p.x-center.x),height=pumpSmoothHeight(distance,bw,n)*dims.y*0.5;
        if(abs(p.y-center.y)<=max(0.5,height))
        {
            float q=(320.0-(center.x+distance))/(320.0-center.x)+4.0*pumpState.x;
            if(p_colorStyle>2.5 && p_colorStyle<3.5)q=(center.y-abs(p.y-center.y)-top)/max(center.y-top,0.001)+4.0*pumpState.x;
            if(p_colorStyle<0.5)c=mix(p_staticCol1,p_staticCol2,1.0-abs(fract(q)*2.0-1.0));
            else if(p_colorStyle>1.5 && p_colorStyle<3.5)c=pumpRainbow(q);
            else c=p_colorStyle>4.5?pumpScreenColor:pumpHue(p_colorStyle<1.5?pumpState.x:pumpState.y);
        }
    }
    pumpRect(c,p,vec4(0.0,(1.0-pumpLevels.x)*200.0,left,pumpLevels.x*200.0),vec4(0.0,0.0,0.0,200.0),1.0,0.0,bg,false);
    pumpRect(c,p,vec4(290.0,0.0,30.0,top/3.0),vec4(320.0,0.0,290.0,0.0),1.0,0.0,bg,false);
    pumpRect(c,p,vec4(290.0,2.0*top/3.0,30.0,top/3.0),vec4(320.0,0.0,290.0,0.0),1.0,0.0,bg,false);
    if(top<remaining)pumpRect(c,p,vec4(left,0.0,remaining*pumpLevels.z,top),vec4(left+remaining,0.0,left,0.0),1.0,1.0,bg,false);
    else pumpRect(c,p,vec4(left,top*(1.0-pumpLevels.z),remaining,top*pumpLevels.z),vec4(0.0,top,0.0,0.0),1.0,0.0,bg,false);
    pumpRect(c,p,vec4(250.0-top,0.0,40.0,top),vec4(290.0-top,0.0,250.0-top,0.0),pumpLevels.y,0.0,bg,false);
    pumpRect(c,p,vec4(left+remaining,0.0,40.0,top),vec4(left+remaining+40.0,0.0,left+remaining,0.0),pumpLevels.w,0.0,bg,false);
    pumpRect(c,p,vec4(290.0,top/3.0,30.0,top/3.0),vec4(320.0,0.0,290.0,0.0),pumpLevels.w,0.0,bg,false);
    fragColor=vec4(clamp(c,0.0,1.0),1.0);
}
