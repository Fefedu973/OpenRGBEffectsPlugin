// Concentric spectrum. Radius and origin remain in the source canvas units.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    float radius = distance(canvas, vec2(p_xPos, p_yPos));
    float direction = p_reverse > 0.5 ? 1.0 : -1.0;
    float hue = (1.0 + direction * 3.0 * t_speed
                + radius / (max(p_scale, 0.0) + 0.1)) / 360.0;
    float edge = 1.0 - smoothstep(359.0, 360.0, radius);
    fragColor = vec4(HSVToRGB(vec3(fract(hue), 1.0, 1.0)) * edge, 1.0);
}
