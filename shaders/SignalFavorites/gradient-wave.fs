// A moving, repeating RGB-stop gradient. HSL hue rotation preserves lightness.
vec3 sfWaveHueRotate(vec3 color, float turns)
{
    float high = max(color.r, max(color.g, color.b));
    float low = min(color.r, min(color.g, color.b));
    float chroma = high - low;
    if(chroma < 0.000001) return color;
    float hue;
    if(high == color.r) hue = (color.g - color.b) / chroma;
    else if(high == color.g) hue = 2.0 + (color.b - color.r) / chroma;
    else hue = 4.0 + (color.r - color.g) / chroma;
    return vec3(low) + chroma * HSVToRGB(vec3(fract(hue/6.0 + turns), 1.0, 1.0));
}
vec3 sfWavePalette(int index)
{
    vec3 color = p_color1;
    if(index == 1) color = p_color2;
    if(index == 2) color = p_color3;
    if(index == 3) color = p_color4;
    if(p_colorCycle > 0.5) color = sfWaveHueRotate(color, t_colorCycleSpeed / 300.0);
    return color;
}
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    int direction = int(floor(p_effectType + 0.5));
    bool vertical = direction >= 2;
    float size = 1.0 + 0.05 * (clamp(p_peakSize, 1.0, 100.0) - 1.0);
    float along = (vertical ? canvas.y : canvas.x) / size;
    if(direction == 1) along = 320.0 - along;
    if(direction == 3) along = 200.0 - along;
    int count = int(clamp(floor(p_nColors + 0.5), 2.0, 4.0));
    float period = count == 4 ? 320.0 : (count == 3 ? 210.0 : (vertical ? 175.0 : 128.0));
    float span = (count == 2 && vertical) ? 440.0 : 640.0;
    float offset = mod(6.0 * t_speed, period);
    float position = clamp((along - offset + span * 0.5) / span, 0.0, 1.0);
    float segment;
    if(count == 3)
        segment = position < 0.888 ? position / 0.111 : 8.0 + (position - 0.888) / 0.112;
    else
        segment = position * (count == 4 ? 8.0 : (vertical ? 5.0 : 10.0));
    int first = int(mod(floor(segment), float(count)));
    int second = int(mod(float(first + 1), float(count)));
    fragColor = vec4(mix(sfWavePalette(first), sfWavePalette(second), fract(segment)), 1.0);
}
