// SPDX-License-Identifier: GPL-2.0-or-later
// Original radial reconstruction. Birth color is selected in integrated travel
// space; the native preset documents the source's finite-ring/control history.
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 p=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y)*vec2(320.0,200.0);
    float radius=length(p-vec2(p_xPos,p_yPos));
    float travel=1.5*t_speed;
    float distanceFromBirth=travel-max(0.0,radius-1.0);
    float phase=mod(max(0.0,distanceFromBirth),3.0*max(1.0,p_scale));
    vec3 band=phase<=p_scale?p_hue3:(phase<=2.0*p_scale?p_hue1:p_hue2);
    float aa=max(fwidth(radius),0.35);
    float cover=1.0-smoothstep(min(travel+3.5,402.5)-aa,min(travel+3.5,402.5)+aa,radius);
    fragColor=vec4(mix(p_hue1,band,cover),1.0);
}
