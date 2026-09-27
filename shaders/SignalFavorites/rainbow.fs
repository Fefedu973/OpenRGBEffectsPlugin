// One hue degree per reference pixel; zero speed leaves a spatial rainbow.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    float along = p_vertical > 0.5 ? canvas.y : canvas.x;
    float hue = fract((along - 1.0 - 6.0 * t_speed) / 360.0);
    fragColor = vec4(HSVToRGB(vec3(hue, 1.0, 1.0)), 1.0);
}
