// Moving overlapping rings, evaluated per pixel without a browser or history texture.
vec3 sfTunnelHSL(float hue, float saturation, float lightness)
{
    float chroma = (1.0 - abs(2.0 * lightness - 1.0)) * saturation;
    return vec3(lightness - chroma * 0.5) + chroma * HSVToRGB(vec3(fract(hue), 1.0, 1.0));
}
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    vec3 color = vec3(0.0);
    float softness = max(0.25, 0.5 * length(vec2(320.0, 200.0) / iResolution.xy));
    // Fifty rings correspond to the 200px reference height, independent of output size.
    for(int i = 1; i < 50; ++i)
    {
        float ring = float(i);
        float angle = (5.0 * ring + 3.0 * t_speed) * 0.01745329252;
        vec2 center = vec2(160.0, 100.0) + 200.0 * vec2(
            cos(angle) * sin(angle/5.0), sin(angle) * cos(angle/3.0));
        float distanceToStroke = abs(length(canvas - center) - ring * p_size);
        float coverage = 1.0 - smoothstep(15.0-softness, 15.0+softness, distanceToStroke);
        vec3 ringColor = sfTunnelHSL(ring/36.0, p_sat/100.0, p_lite/100.0);
        color = mix(color, ringColor, coverage);
    }
    fragColor = vec4(color, 1.0);
}
