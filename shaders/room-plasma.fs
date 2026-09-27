// SPDX-License-Identifier: GPL-2.0-or-later
// Smooth spatial plasma; original Room preset.
#define FLOW_SPEED 0.35
#define SPATIAL_SCALE 8.0
void mainImage(out vec4 color, in vec2 pixel)
{
    vec2 uv = pixel / iResolution.xy;
    float t = iTime * FLOW_SPEED;
    float field = sin(uv.x * SPATIAL_SCALE + t)
        + sin(uv.y * SPATIAL_SCALE * 1.3 - t * 1.2)
        + sin(length(uv - vec2(0.5)) * SPATIAL_SCALE * 2.0 - t);
    color = vec4(HSVToRGB(vec3(fract(field * 0.16 + t * 0.06), 0.87, 0.8)), 1.0);
}
