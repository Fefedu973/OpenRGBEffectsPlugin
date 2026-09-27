// SPDX-License-Identifier: GPL-2.0-or-later
// Original procedural reconstruction; no upstream code or texture assets.
float gxHash(float n)
{
    return fract(sin(n*23.713+17.193)*17381.713);
}

vec3 gxHsl(float hue, float saturation, float lightness)
{
    float chroma = (1.0-abs(2.0*lightness-1.0))*saturation;
    float value = lightness+0.5*chroma;
    return HSVToRGB(vec3(fract(hue/360.0),chroma/max(value,0.000001),value));
}

float gxDisc(vec2 point, vec2 center, float radius)
{
    float distance = length(point-center);
    float edge = max(fwidth(distance),0.06);
    return 1.0-smoothstep(radius-edge,radius+edge,distance);
}

vec3 gxCloud(vec3 background, vec2 point, vec2 center, float radius, vec3 ink, float alpha)
{
    return mix(background,ink,clamp(alpha*gxDisc(point,center,radius),0.0,1.0));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 point = vec2(fragCoord.x,iResolution.y-fragCoord.y)/iResolution.xy*vec2(320.0,200.0);
    vec3 rgb = p_backColor;
    // The surrounding field is not transformed with the galaxy.
    for(int star=0;star<50;++star)
    {
        float seed = float(star)+901.0;
        float phase = fract(t_effectSpeed*(0.0015+0.0005*gxHash(seed+3.0))+gxHash(seed+9.0));
        float opacity = 0.75*(1.0-abs(phase*2.0-1.0));
        vec2 center = vec2(320.0*gxHash(seed),200.0*gxHash(seed+11.0));
        rgb = gxCloud(rgb,point,center,1.0+10.0*gxHash(seed+19.0)*opacity,
                      gxHsl(360.0*gxHash(seed+29.0),0.25,0.75),opacity);
    }

    // Preserve the source transform order: local edge compression, then scale
    // and clockwise canvas rotation around the user-selected pivot.
    if(p_globalGScale>0.0 && p_globalYScale>0.0)
    {
        vec2 pivot = vec2(p_globalXPos*3.2,p_globalYPos*2.0);
        float rotation = p_globalRotation*0.062831853;
        float co = cos(rotation), si = sin(rotation);
        vec2 delta = (point-pivot)/(p_globalGScale/50.0);
        vec2 local = pivot+vec2(co*delta.x+si*delta.y,-si*delta.x+co*delta.y);
        local -= vec2(160.0,100.0);
        local.y /= p_globalYScale/100.0;
        float radial = length(local);
        float time = t_effectSpeed*0.012;
        float kind = floor(p_effectType+0.5);

        if(kind<0.5) // Andromeda: blue-green body, warm orbital clouds.
        {
            rgb = mix(rgb,gxHsl(mix(240.0,120.0,clamp(radial/55.0,0.0,1.0)),1.0,0.4),
                      0.8*(1.0-smoothstep(10.0,110.0,radial)));
        }
        else if(kind<1.5) // Antennae: offset cyan body with a warm tidal arm.
        {
            float r = length(local-vec2(-80.0,50.0));
            rgb = mix(rgb,gxHsl(190.0,0.5,0.9),1.0-smoothstep(10.0,128.0,r));
        }
        else if(kind<2.5) // Condor: two expanding arms around a pink core.
        {
            float limit = 128.0+16.0*sin(t_effectSpeed/50.0);
            rgb = mix(rgb,gxHsl(290.0,1.0,0.8),1.0-smoothstep(10.0,limit,radial));
        }
        else if(kind<3.5) // Hoag: a separated blue ring and yellow nucleus.
        {
            float ringDistance = (radial-98.0)/18.0;
            float ring = exp(-ringDistance*ringDistance)*0.32;
            rgb = mix(rgb,gxHsl(215.0,0.8,0.85),ring);
            rgb = mix(rgb,gxHsl(60.0,1.0,0.80+0.19*sin(t_effectSpeed/50.0)),
                      1.0-smoothstep(10.0,49.0,radial));
        }
        else // Milky Way: white center, blue halo, warm pink orbital clouds.
        {
            float haloDistance = (radial-85.0)/36.0;
            float halo = 0.60*exp(-haloDistance*haloDistance);
            rgb = mix(rgb,gxHsl(200.0,1.0,0.5),halo);
            rgb = mix(rgb,vec3(1.0),1.0-smoothstep(10.0,107.0,radial));
        }

        for(int cloud=0;cloud<100;++cloud)
        {
            float seed = float(cloud)+43.0;
            float random = gxHash(seed);
            float angle = 6.2831853*gxHash(seed+13.0)+time*(0.5+0.5*gxHash(seed+31.0));
            float orbit, radius, opacity;
            vec2 center;
            vec3 ink;
            if(kind<0.5)
            {
                orbit = 50.0+40.0*floor(random*3.0);
                center = vec2(cos(angle),sin(angle))*orbit;
                radius = 10.0+15.0*gxHash(seed+47.0);
                opacity = 0.3+0.3*gxHash(seed+59.0);
                ink = gxHsl(60.0-orbit*0.5,1.0,0.4);
            }
            else if(kind<1.5)
            {
                if(cloud<50)
                {
                    orbit = 30.0+20.0*floor(random*3.0);
                    angle *= 4.0;
                    center = vec2(-80.0,50.0)+vec2(cos(angle),sin(angle))*orbit;
                    radius = 5.0+15.0*gxHash(seed+47.0);
                    opacity = 0.2+0.3*gxHash(seed+59.0);
                    ink = gxHsl(300.0+120.0*gxHash(seed+71.0),0.8,0.7);
                }
                else
                {
                    float age = mod(time+6.0*random,6.0);
                    orbit = (exp(age*0.66)-1.0)*8.0;
                    angle = 1.5707963+age*(0.8+0.8*gxHash(seed+31.0));
                    center = vec2(-80.0,50.0)+vec2(cos(angle),sin(angle))*orbit;
                    radius = 5.0+20.0*gxHash(seed+47.0)+age*6.0;
                    opacity = smoothstep(0.0,0.25,age)*(1.0-smoothstep(300.0,400.0,orbit));
                    ink = gxHsl(60.0*gxHash(seed+71.0),1.0,0.4);
                }
            }
            else if(kind<2.5)
            {
                float age = mod(time*1.3+6.0*random,6.0);
                orbit = exp(age)-1.0;
                angle = 1.5707963+3.14159265*float(cloud-2*(cloud/2))
                      +age*(0.5+0.5*gxHash(seed+31.0));
                center = vec2(cos(angle),sin(angle))*orbit;
                radius = 20.0+30.0*gxHash(seed+47.0);
                opacity = (0.3+0.3*gxHash(seed+59.0))*smoothstep(0.0,0.2,age)
                          *(1.0-smoothstep(350.0,420.0,orbit));
                ink = gxHsl(-10.0+30.0*gxHash(seed+71.0),0.5+0.4*random,0.5+0.2*gxHash(seed+83.0));
            }
            else if(kind<3.5)
            {
                orbit = 80.0+20.0*floor(random*3.0);
                center = vec2(cos(angle),sin(angle))*orbit;
                radius = 1.0+15.0*gxHash(seed+47.0);
                opacity = 0.7+0.3*gxHash(seed+59.0);
                ink = gxHsl(200.0,1.0,0.5+0.5*gxHash(seed+71.0));
            }
            else
            {
                orbit = 50.0+20.0*floor(random*3.0);
                center = vec2(cos(angle),sin(angle))*orbit;
                radius = 5.0+10.0*gxHash(seed+47.0);
                opacity = 0.3+0.3*gxHash(seed+59.0);
                ink = gxHsl(330.0+60.0*gxHash(seed+71.0),1.0,0.4);
            }
            rgb = gxCloud(rgb,local,center,radius,ink,opacity);
        }
    }
    fragColor = vec4(clamp(rgb,0.0,1.0),1.0);
}
