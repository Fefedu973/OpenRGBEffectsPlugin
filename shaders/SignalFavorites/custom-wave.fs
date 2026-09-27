// SPDX-License-Identifier: GPL-2.0-or-later
uniform vec4 prState;
void mainImage(out vec4 color,in vec2 position)
{
    vec2 point=vec2(position.x/iResolution.x,1.0-position.y/iResolution.y)*vec2(320.0,200.0);
    float band=floor(((p_bVertical>.5?point.y:point.x)-prState.x)/50.0);
    float count=floor(p_nColors+.5),index=mod(band,count);
    vec3 rgb=p_color1;
    if(count<2.5){if(index<.5)rgb=p_color2;}
    else if(count<3.5){if(index<.5)rgb=p_color3;else if(index<1.5)rgb=p_color2;}
    else {if(index>.5&&index<1.5)rgb=p_color2;else if(index<2.5&&index>1.5)rgb=p_color3;else if(index>2.5)rgb=p_color4;}
    color=vec4(rgb,1.0);
}
