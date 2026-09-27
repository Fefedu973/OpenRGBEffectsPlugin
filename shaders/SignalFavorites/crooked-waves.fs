// SPDX-License-Identifier: GPL-2.0-or-later
// Rotate the sampling point into the stripe frame; positions and shared random
// color come from the bounded native four-stripe simulation.
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 p=vec2(fragCoord.x/iResolution.x,1.0-fragCoord.y/iResolution.y)*vec2(320.0,200.0);
    float axis=(p.x+p.y)*0.7071067811865475;
    float aa=max(fwidth(axis),0.25),cover=0.0;
    for(int i=0;i<4;++i)
    {
        if(float(i)>=p_barAmount)break;
        float position=bBarPositions[i];
        float d=abs(axis-(position+p_barWidth*0.5));
        cover=max(cover,1.0-smoothstep(p_barWidth*.5-aa,p_barWidth*.5+aa,d));
    }
    fragColor=vec4(mix(p_bgColor,bBarColor,cover),1.0);
}
