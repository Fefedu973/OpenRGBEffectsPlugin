// Analytic linear gradient with stable stop ordering, including coincident stops.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    vec2 start = vec2(p_startX, p_startY);
    vec2 axis = vec2(p_endX, p_endY) - start;
    float denominator = dot(axis, axis);
    if(denominator < 0.000001)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float along = dot(canvas - start, axis) / denominator;
    float positions[4];
    positions[0] = p_color1pos / 320.0;
    positions[1] = p_color2pos / 320.0;
    positions[2] = p_color3pos / 320.0;
    positions[3] = p_color4pos / 320.0;
    vec3 colors[4];
    colors[0] = p_color1;
    colors[1] = p_color2;
    colors[2] = p_color3;
    colors[3] = p_color4;
    int count = int(clamp(floor(p_numColors + 0.5), 2.0, 4.0));
    // Find the last left stop and first right stop without requiring UI sorting.
    float left = -1e10;
    float right = 1e10;
    vec3 leftColor = vec3(0.0);
    vec3 rightColor = vec3(0.0);
    for(int i = 0; i < 4; ++i)
    {
        if(i >= count) break;
        float pos = positions[i];
        if(pos <= along && pos >= left) { left = pos; leftColor = colors[i]; }
        if(pos > along && pos < right) { right = pos; rightColor = colors[i]; }
    }
    vec3 color;
    if(left < -1e9) color = rightColor;
    else if(right > 1e9) color = leftColor;
    else color = mix(leftColor, rightColor, clamp((along-left)/(right-left), 0.0, 1.0));
    fragColor = vec4(color, 1.0);
}
