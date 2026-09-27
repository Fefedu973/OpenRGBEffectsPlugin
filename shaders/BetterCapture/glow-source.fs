// SPDX-License-Identifier: GPL-2.0-or-later
void mainImage(out vec4 color,in vec2 p){
 vec2 point=vec2(p.x,iResolution.y-p.y)-vec2(bDomain.x);
 color=image0(point/vec2(320.0,200.0));
 if(bGlow.z>0.0)color*=clamp(5.0*(dot(unassociated(color),vec3(.2126,.7152,.0722))-bGlow.z),0.0,1.0);
 if(bGlow.w>.5)color=saturatePremultiplied(color,bGlow.x);
}
