// SPDX-License-Identifier: GPL-2.0-or-later
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 p=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y);
    int quadrant=p.y<0.5?(p.x<0.5?0:1):(p.x<0.5?3:2);
    float breathe=quadrant==0?p_breathing1:(quadrant==1?p_breathing2:(quadrant==2?p_breathing3:p_breathing4));
    fragColor=vec4(bQuadrant[quadrant]*(1.0-(breathe>0.5?bBreathTint:0.0)),1.0);
}
