// SPDX-License-Identifier: GPL-2.0-or-later
uniform vec4 prState;
uniform vec3 prColor;
void mainImage(out vec4 color,in vec2 position)
{
    vec2 uv=position/iResolution.xy;
    vec2 point=vec2(uv.x,1.0-uv.y)*vec2(320.0,200.0);
    vec4 old=texture2D(iPreviousFrame,uv);
    vec3 rgb=mix(old.a>.5?old.rgb:vec3(0.0),p_bgColor,1.0-p_trail/100.0);
    float axis=p_vertical>.5?point.x:point.y;
    if(axis>=prState.x&&axis<prState.x+p_barWidth)rgb=prColor;
    color=vec4(rgb,1.0);
}
