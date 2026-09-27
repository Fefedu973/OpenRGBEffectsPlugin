// Alternating sweeps reconstructed from travel distance, not a mutable framebuffer.
float sfSweepHash(float value)
{
    return fract(sin(value * 127.1 + 73.7) * 43758.5453);
}
vec3 sfSweepColor(float event)
{
    vec3 color = mod(event, 2.0) < 1.0 ? p_color2 : p_color1;
    if(p_rainbow < 0.5) return color;
    float low = min(color.r, min(color.g, color.b));
    float high = max(color.r, max(color.g, color.b));
    return vec3(low) + (high-low) * HSVToRGB(vec3(sfSweepHash(event+1.0), 1.0, 1.0));
}
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    float travelled = max(0.0, 6.0 * t_speed);
    float event;
    float covered;
    int direction = int(floor(p_direction + 0.5));
    if(direction < 2)
    {
        float extent = direction == 0 ? 320.0 : 200.0;
        event = floor(travelled / extent);
        float front = mod(travelled, extent);
        float position = direction == 0 ? canvas.x : canvas.y;
        if(mod(event, 2.0) > 0.5) position = extent - position;
        covered = 1.0 - step(front, position);
    }
    else
    {
        float phase = mod(travelled, 1040.0);
        float stage;
        float position;
        float front;
        if(phase < 200.0) { stage=0.0; position=canvas.y; front=phase; }
        else if(phase < 520.0) { stage=1.0; position=320.0-canvas.x; front=phase-200.0; }
        else if(phase < 720.0) { stage=2.0; position=200.0-canvas.y; front=phase-520.0; }
        else { stage=3.0; position=canvas.x; front=phase-720.0; }
        event = floor(travelled/1040.0) * 4.0 + stage;
        covered = 1.0 - step(front, position);
    }
    vec3 previous = event < 1.0 ? p_color1 : sfSweepColor(event-1.0);
    fragColor = vec4(mix(previous, sfSweepColor(event), covered), 1.0);
}
