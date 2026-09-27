// SPDX-License-Identifier: GPL-2.0-or-later
// Original procedural GPU reconstruction. No source effect or assets embedded.
float spHash(float n)
{
    return fract(sin(n * 17.137 + 3.117) * 19417.731);
}

vec3 spHsl(vec3 rgb)
{
    float hi = max(rgb.r, max(rgb.g, rgb.b));
    float lo = min(rgb.r, min(rgb.g, rgb.b));
    float d = hi - lo;
    float l = (hi + lo) * 0.5;
    float h = 0.0;
    if(d > 0.000001)
    {
        if(hi == rgb.r) h = (rgb.g - rgb.b) / d;
        else if(hi == rgb.g) h = 2.0 + (rgb.b - rgb.r) / d;
        else h = 4.0 + (rgb.r - rgb.g) / d;
    }
    return vec3(fract(h / 6.0), d / max(1.0 - abs(2.0*l - 1.0), 0.000001), l);
}

vec3 spRgb(vec3 hsl)
{
    float chroma = (1.0 - abs(2.0*hsl.z - 1.0)) * hsl.y;
    float value = hsl.z + chroma * 0.5;
    return HSVToRGB(vec3(fract(hsl.x), chroma / max(value,0.000001), value));
}

float spDisc(vec2 point, vec2 center, float radius)
{
    float d = length(point - center);
    float edge = max(fwidth(d), 0.06);
    return 1.0 - smoothstep(radius-edge, radius+edge, d);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 point = vec2(fragCoord.x, iResolution.y-fragCoord.y) / iResolution.xy * vec2(320.0,200.0);
    float hueShift = p_colorCycle > 0.5 ? t_cycleSpeed / 300.0 : 0.0;
    vec3 background = spHsl(p_backColor);
    background.x += hueShift;
    vec3 rgb = spRgb(background);

    // Four overlapping cloud depths; their immutable seeds survive controls.
    for(int layer=0; layer<4; ++layer)
    {
        float depth = float(layer);
        vec3 ink = spRgb(vec3((280.0+30.0*depth)/360.0+hueShift,1.0,0.30));
        for(int cloud=0; cloud<15; ++cloud)
        {
            float seed = float(cloud + layer*15) + 31.0;
            vec2 center = vec2(320.0*spHash(seed),100.0+(spHash(seed+101.0)-0.5)*100.0/(depth+1.0));
            float radius = max(1.0,(50.0-depth*10.0)*p_backMod/50.0
                               +20.0*sin(t_effectSpeed/275.0+6.2831853*spHash(seed+227.0)));
            rgb = mix(rgb,ink,0.10*spDisc(point,center,radius));
        }
    }

    vec3 starInk = spHsl(p_starColor);
    starInk.x += hueShift;
    vec3 starRgb = spRgb(starInk);
    if(p_starSize > 0.0)
    {
        for(int star=0; star<50; ++star)
        {
            float seed = float(star) + 503.0;
            float lifetime = 1.4 + 1.7*spHash(seed+11.0);
            float age = mod(t_effectSpeed*0.006 + lifetime*spHash(seed+37.0),lifetime);
            vec2 ray = vec2(spHash(seed)-0.5,spHash(seed+19.0)-0.5)*vec2(160.0,100.0);
            vec2 center = vec2(160.0,100.0)+ray*exp(age);
            float visible = smoothstep(0.0,0.25,age) * (1.0-smoothstep(lifetime-0.05,lifetime,age));
            rgb = mix(rgb,starRgb,0.5*visible*spDisc(point,center,p_starSize*0.25));
        }
    }
    fragColor = vec4(clamp(rgb,0.0,1.0),1.0);
}
