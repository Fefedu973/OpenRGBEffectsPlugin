// SPDX-License-Identifier: GPL-2.0-or-later
uniform vec4 prState;
vec3 pinHue(float h){return clamp(abs(fract(h+vec3(0,2.0/3.0,1.0/3.0))*6.0-3.0)-1.0,0.0,1.0);}
float pinLine(vec2 p,vec2 a,vec2 b)
{
    vec2 v=b-a;float t=dot(p-a,v)/max(dot(v,v),.00001);
    if(t<0.0||t>1.0)return 10000.0; // Canvas default butt caps.
    return length(p-a-v*t);
}
void mainImage(out vec4 color,in vec2 position)
{
    vec2 point=vec2(position.x/iResolution.x,1.0-position.y/iResolution.y)*vec2(320.0,200.0);
    vec2 origin=prState.yz;
    vec2 circle=vec2(160.0,p_bounce>.5?160.0:100.0);
    vec3 rgb=p_background;float aa=max(.15,160.0/iResolution.x);
    // Preserve the repeated 260-degree ray and fixed target circle, including
    // the different Y center used by the original edge-to-edge mode.
    for(int i=0;i<37;++i)
    {
        vec3 ink=p_rainbow>.5?pinHue(float(i)/36.0):p_pinwheelColor;
        float a=radians(prState.x+10.0*(i<26?float(i+1):float(i)));
        vec2 end=circle+200.0*vec2(cos(a),sin(a));
        float disk=1.0-smoothstep(5.0-aa,5.0+aa,length(point-origin));
        rgb=mix(rgb,ink,disk);
        float line=1.0-smoothstep(p_lineThickness*.5-aa,p_lineThickness*.5+aa,pinLine(point,origin,end));
        rgb=mix(rgb,ink,line);
    }
    color=vec4(rgb,1.0);
}
