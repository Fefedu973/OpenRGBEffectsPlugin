// Alpha-composited sine breathing against black, expressed directly as RGB.
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    float alpha = p_breathe > 0.5 ? 0.5 + 0.5*sin(t_speed * 0.0471238898038) : 1.0;
    fragColor = vec4(p_color * alpha, 1.0);
}
