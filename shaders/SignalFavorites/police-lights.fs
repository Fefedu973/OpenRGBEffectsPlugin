// SPDX-License-Identifier: GPL-2.0-or-later
// The fixed-step native state owns quantized timing, including the 0..4 stop.
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    float x=fragCoord.x/iResolution.x*320.0;
    bool left=x<106.0,right=x>=214.0;
    vec3 color=vec3(0.0);
    if(bPoliceTiming>60.0&&bPoliceTiming<100.0)
    {if(left)color=p_color1;else if(right)color=p_color3;}
    else if(bPoliceTiming>10.0&&bPoliceTiming<50.0)
    {if(left)color=p_color3;else if(right)color=p_color1;}
    else if(!left&&!right)color=p_color2;
    fragColor=vec4(color,1.0);
}
