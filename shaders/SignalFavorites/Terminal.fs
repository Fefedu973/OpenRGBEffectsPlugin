// SPDX-License-Identifier: GPL-2.0-or-later
// Independent glyph-rain renderer. A bounded temporal reconstruction avoids a
// feedback texture, hidden frame-rate dependence or per-device CPU history.
uint terminalHash(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    return value ^ (value >> 16u);
}
float terminalUnit(uint value)
{
    return float(terminalHash(value) & 65535u) / 65536.0;
}

// Hand-drawn uppercase 5x7 alphabet, A through Y. Six rows occupy the first
// 30 bits (leftmost pixel is bit4); the final row is the second component.
// These patterns are original data, not a browser font or a copied font asset.
const uvec2 terminalLetters[25] = uvec2[25](
    uvec2(589284910u,17u), uvec2(589252158u,30u), uvec2(587743790u,14u),
    uvec2(622380636u,28u), uvec2(554648095u,31u), uvec2(554648095u,16u),
    uvec2(589021742u,14u), uvec2(589284913u,17u), uvec2(138547359u,31u),
    uvec2(622921799u,12u), uvec2(625758801u,17u), uvec2(554189328u,31u),
    uvec2(588961649u,17u), uvec2(588896049u,17u), uvec2(588826158u,14u),
    uvec2(554649150u,16u), uvec2(626574894u,13u), uvec2(625952318u,17u),
    uvec2(35078671u,30u), uvec2(138547359u,4u), uvec2(588826161u,14u),
    uvec2(353945137u,4u), uvec2(727369265u,10u), uvec2(581052977u,17u),
    uvec2(138553905u,4u)
);
float terminalGlyph(int letter, vec2 local)
{
    ivec2 cell = ivec2(floor(local));
    if(cell.x < 0 || cell.x >= 5 || cell.y < 0 || cell.y >= 7) return 0.0;
    uvec2 pattern = terminalLetters[letter];
    uint row = cell.y == 6 ? pattern.y : (pattern.x >> uint(5 * cell.y)) & 31u;
    return float((row >> uint(4 - cell.x)) & 1u);
}
float terminalHead(float tick, float initial, float phase, float omega, float gap)
{
    // The displacement between adjacent ticks is 3+2*cos(...), hence lies
    // in [1,5] reference pixels. Summing this walk analytically keeps history
    // independent of how often OpenRGB happens to render a frame.
    float distance = 3.0 * tick
                   + (sin(phase + omega * tick) - sin(phase)) / sin(omega * 0.5);
    float head = initial + distance;
    return head < 200.0 ? head : mod(head - 200.0, 200.0 + gap) - gap;
}
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 canvas = vec2(fragCoord.x, iResolution.y - fragCoord.y)
                * vec2(320.0, 200.0) / iResolution.xy;
    float size = clamp(p_fontSize, 5.0, 12.0);
    uint column = uint(floor(canvas.x / size));
    float localX = (canvas.x - float(column) * size) * 8.0 / size;
    float tick = floor(max(0.0, iTime) * 50.0);
    float initial = -500.0 * terminalUnit(column + 11u);
    float phase = 6.28318530718 * terminalUnit(column + 113u);
    float omega = 0.71 + 0.93 * terminalUnit(column + 331u);
    float gap = 1.0 + 99.0 * terminalUnit(column + 719u);
    float light = 0.0;
    if(localX < 5.0)
    {
        // Composite oldest to newest just as successive black alpha0.15 fills
        // and opaque glyph draws would. Omitted history is below 0.0031% full
        // scale after 64 ticks, far less than one 8-bit output level.
        for(int age = 63; age >= 0; --age)
        {
            float historicalTick = tick - float(age);
            light *= 0.85;
            if(historicalTick >= 0.0)
            {
                float head = terminalHead(historicalTick, initial, phase, omega, gap);
                float localY = (canvas.y - head) * 8.0 / size + 7.0;
                if(localY >= 0.0 && localY < 7.0)
                {
                    int letter = int(terminalHash(column * 65537u + uint(historicalTick) + 971u) % 25u);
                    light = mix(light, 1.0, terminalGlyph(letter, vec2(localX, localY)));
                }
            }
        }
    }
    fragColor = vec4(p_color * light, 1.0);
}
