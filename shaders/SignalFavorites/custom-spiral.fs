// Alternating color sectors; temporal scale is calibrated to the 60 Hz reference.
vec3 sfSpiralPalette(int index)
{
    if(index == 0) return p_color1;
    if(index == 1) return p_color2;
    if(index == 2) return p_color3;
    return p_color4;
}
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    vec2 delta = canvas - vec2(p_xPos, p_yPos);
    float direction = p_reversed > 0.5 ? -1.0 : 1.0;
    float directionAngle = dot(delta, delta) > 0.000001 ? atan(delta.y, delta.x) / 6.28318530718 : 0.0;
    float angle = fract(directionAngle
                      - direction * t_speed / 60.0);
    int count = int(clamp(floor(p_numColors + 0.5), 2.0, 4.0));
    float sectors = count == 3 ? 6.0 : 8.0;
    int index = int(mod(floor(angle * sectors), float(count)));
    fragColor = vec4(sfSpiralPalette(index), 1.0);
}
