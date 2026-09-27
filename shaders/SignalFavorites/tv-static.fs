// SPDX-License-Identifier: GPL-2.0-or-later
// Stable integer cell identity, refreshed at a nominal 60 Hz. Original noise.
float staticHash(vec3 p)
{p=fract(p*vec3(0.1031,0.11369,0.13787));p+=dot(p,p.yzx+19.19);return fract((p.x+p.y)*p.z);}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 p=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y)*vec2(320.0,200.0);
    vec3 cell=vec3(floor(p/max(p_pixelSize,1.0)),floor(iTime*60.0));
    float value=staticHash(cell);
    vec3 color;
    if(p_customColor>1.5)
    {float slot=floor(value*3.0);color=slot<1.0?p_color1:(slot<2.0?p_color2:p_color3);}
    else if(p_customColor>0.5)color=value<0.5?p_color1:p_color2;
    else
    {
        color=HSVToRGB(vec3(floor(value*360.0)/360.0,1.0,1.0));
        float whiteDraw=floor(staticHash(cell+vec3(13.7,41.3,7.9))*(p_whiteAmount+1.0)+0.5);
        if(whiteDraw<p_whiteAmount)color=vec3(1.0);
    }
    fragColor=vec4(color,1.0);
}
