// Original analytic angular spectrum, in the reference 320 x 200 canvas.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    vec2 delta = canvas - vec2(p_xPos, p_yPos);
    float angle = dot(delta, delta) > 0.000001 ? atan(delta.y, delta.x) / 6.28318530718 : 0.0;
    float direction = p_reversed > 0.5 ? -1.0 : 1.0;
    float hue = fract(angle - direction * t_speed / 180.0);
    fragColor = vec4(HSVToRGB(vec3(hue, 1.0, 1.0)), 1.0);
}
