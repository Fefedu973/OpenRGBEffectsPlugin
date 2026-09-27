// SPDX-License-Identifier: GPL-2.0-or-later
// Global white impulse over a horizontal or vertical one-degree-per-pixel wave.
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 p=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y)*vec2(320.0,200.0);
    float coordinate=p_vertical>0.5?p.y:p.x;
    vec3 color=HSVToRGB(vec3(fract((floor(coordinate)-1.0-50.0*t_speed)/360.0),1.0,1.0));
    float white=max(0.0,0.5-0.5*iTime);
    if(p_pulse>0.5&&iTap.w>0.0&&iTap.z>=0.0)
        white=clamp(p_pulseIntensity/100.0-0.5*iTap.z,0.0,1.0);
    fragColor=vec4(mix(color,vec3(1.0),white),1.0);
}
