// SPDX-License-Identifier: GPL-2.0-or-later
// Original spatially indexed GPU shimmer field; no Canvas code or assets.
float uwHash(vec2 cell)
{
    return fract(sin(dot(cell,vec2(13.173,73.791))+5.31)*31973.113);
}

vec3 uwHsl(vec3 rgb)
{
    float hi=max(rgb.r,max(rgb.g,rgb.b)),lo=min(rgb.r,min(rgb.g,rgb.b));
    float d=hi-lo,l=(hi+lo)*0.5,h=0.0;
    if(d>0.000001)
    {
        if(hi==rgb.r)h=(rgb.g-rgb.b)/d;
        else if(hi==rgb.g)h=2.0+(rgb.b-rgb.r)/d;
        else h=4.0+(rgb.r-rgb.g)/d;
    }
    return vec3(fract(h/6.0),d/max(1.0-abs(2.0*l-1.0),0.000001),l);
}

vec3 uwRgb(vec3 hsl)
{
    float c=(1.0-abs(2.0*hsl.z-1.0))*hsl.y;
    float v=hsl.z+c*0.5;
    return HSVToRGB(vec3(fract(hsl.x),c/max(v,0.000001),v));
}

float uwRing(vec2 point,vec2 center,float radius,float width)
{
    float d=abs(length(point-center)-radius);
    return 1.0-smoothstep(width*0.5-0.5,width*0.5+0.5,d);
}

void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 point=vec2(fragCoord.x,iResolution.y-fragCoord.y)/iResolution.xy*vec2(320.0,200.0);
    vec3 base=uwHsl(p_shimmerColor);
    vec3 rgb=uwRgb(vec3(base.xy,base.z*0.2));
    float sweep=mod(t_speed*1.2,1120.0)-800.0;
    float broad=clamp((point.x-sweep)/800.0,0.0,1.0);
    float wave=sin(broad*6.2831853)*sin(broad*3.14159265);
    rgb=mix(rgb,uwRgb(vec3(base.x-0.13*max(wave,0.0),base.y,base.z*0.25)),abs(wave)*0.55);

    float angle=p_rotation*0.062831853;
    float cs=cos(angle),sn=sin(angle);
    vec2 local=mat2(cs,-sn,sn,cs)*(point-vec2(160.0,100.0))+vec2(160.0,100.0);
    vec2 grid=floor((local+vec2(40.0,100.0))/12.5);
    float density=clamp(p_amount*10.0/1024.0,0.0,1.0);
    float side=p_size*0.25;
    float loss=max(0.0005,0.5-p_fade*0.005);
    float retention=min(1.8,-1.0/(60.0*log(1.0-loss)));
    float trail=min(18.0,p_speed*0.48*retention);
    // Only nearby spatial cells can intersect this fragment. Population is
    // bounded at 1024 sites; these 95 candidates replace a 1000-particle scan.
    if(side>0.0 && density>0.0)
    for(int y=-2;y<=2;++y)
    for(int x=-9;x<=9;++x)
    {
        vec2 cell=grid+vec2(float(x),float(y));
        if(cell.x<0.0||cell.x>=32.0||cell.y<0.0||cell.y>=32.0)continue;
        if(uwHash(cell+vec2(19.0,31.0))>=density)continue;
        float age=mod(t_speed*1.2+200.0*uwHash(cell+vec2(7.0,13.0)),200.0);
        float direction=p_monoDir>0.5?-1.0:(uwHash(cell+vec2(3.0,61.0))<0.5?-1.0:1.0);
        vec2 origin=(cell+vec2(uwHash(cell),uwHash(cell+vec2(29.0,17.0))))*12.5-vec2(40.0,100.0);
        origin.x+=direction*age*0.4;
        vec2 rel=local-origin;
        float along=direction>0.0?rel.x:side-rel.x;
        float xMask=smoothstep(-0.5,0.5,along)*(1.0-smoothstep(side-0.5,side+0.5,along));
        if(along<0.0 && trail>0.01)xMask=max(xMask,exp(along/trail));
        float yMask=smoothstep(-0.5,0.5,rel.y)*(1.0-smoothstep(side-0.5,side+0.5,rel.y));
        float opacity=clamp((1.0-abs(age-100.0)/100.0)*xMask*yMask,0.0,1.0);
        vec3 ink=uwRgb(vec3(base.x-age/1080.0,base.yz));
        rgb=mix(rgb,ink,opacity);
    }

    float rings=p_rippleFreq>0.0?min(32.0,625.0/max(2.0,202.0-2.0*p_rippleFreq)):0.0;
    for(int index=0;index<32;++index)
    {
        float enabled=clamp(rings-float(index),0.0,1.0);
        if(enabled<=0.0)continue;
        vec2 seed=vec2(float(index)+73.0,43.0);
        vec2 center=vec2(uwHash(seed),uwHash(seed+vec2(9.0,27.0)))*vec2(320.0,200.0);
        float radius=mod(t_speed*2.4+500.0*uwHash(seed+vec2(51.0,13.0)),500.0);
        float opacity=enabled*(1.0-abs(radius-250.0)/250.0)*uwRing(point,center,radius,40.0);
        rgb=mix(rgb,uwRgb(vec3(base.x-radius/2520.0,base.yz)),clamp(opacity,0.0,1.0));
    }
    if(p_tapEnable>0.5 && iTap.w>0.5 && iTap.z>=0.0)
    {
        float radius=iTap.z*max(p_speed,0.0)*2.4;
        float envelope=max(0.0,1.0-abs(radius-250.0)/250.0);
        float opacity=envelope*uwRing(point,iTap.xy,radius,10.0);
        rgb=mix(rgb,uwRgb(vec3(base.x-radius/2520.0,base.yz)),clamp(opacity,0.0,1.0));
    }
    fragColor=vec4(clamp(rgb,0.0,1.0),1.0);
}
