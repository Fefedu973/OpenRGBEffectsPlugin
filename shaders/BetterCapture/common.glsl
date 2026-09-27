// SPDX-License-Identifier: GPL-2.0-or-later
// Premultiplied sRGB working textures. Dynamic inputs are top-down; FBO inputs
// use the explicit per-channel flip supplied by the immutable graph manifest.
uniform vec4 bInputFlip,bPlacement,bTone,bGlow,bOptions,bOutputSize;
uniform vec4 bDomain,bKernel,bFieldSize,bContour;
uniform float bPixelated,bMorphStep;
vec2 logicalUV(vec2 p){return vec2(p.x/iResolution.x,1.0-p.y/iResolution.y);}
bool outsideUV(vec2 p){return any(lessThan(p,vec2(0.0)))||any(greaterThan(p,vec2(1.0)));}
vec2 oriented(vec2 p,float flip){return vec2(p.x,mix(p.y,1.0-p.y,flip));}
vec4 image0(vec2 p){return outsideUV(p)?vec4(0.0):texture2D(iChannel0,oriented(p,bInputFlip.x));}
vec4 image1(vec2 p){return outsideUV(p)?vec4(0.0):texture2D(iChannel1,oriented(p,bInputFlip.y));}
vec4 image2(vec2 p){return outsideUV(p)?vec4(0.0):texture2D(iChannel2,oriented(p,bInputFlip.z));}
vec3 unassociated(vec4 c){return c.a>0.00001?clamp(c.rgb/c.a,0.0,1.0):vec3(0.0);}
vec3 cssSaturate(vec3 c,float s){
 return clamp(vec3(dot(c,vec3(.213+.787*s,.715-.715*s,.072-.072*s)),
 dot(c,vec3(.213-.213*s,.715+.285*s,.072-.072*s)),
 dot(c,vec3(.213-.213*s,.715-.715*s,.072+.928*s))),0.0,1.0);
}
vec3 tone(vec3 c){
 float a=radians(bTone.x),co=cos(a),si=sin(a);
 c=clamp(vec3(dot(c,vec3(.213+.787*co-.213*si,.715-.715*co-.715*si,.072-.072*co+.928*si)),
 dot(c,vec3(.213-.213*co+.143*si,.715+.285*co+.140*si,.072-.072*co-.283*si)),
 dot(c,vec3(.213-.213*co-.787*si,.715-.715*co+.715*si,.072+.928*co+.072*si))),0.0,1.0);
 c=clamp(c*bTone.y,0.0,1.0);c=cssSaturate(c,bTone.z);
 if(bTone.w>.5&&bTone.w<1.5){
   vec3 sep=vec3(dot(c,vec3(.393,.769,.189)),dot(c,vec3(.349,.686,.168)),dot(c,vec3(.272,.534,.131)));
   c=clamp(mix(c,sep,.2),0.0,1.0);c=clamp((c-.5)*1.1+.5,0.0,1.0);c=clamp(c*.9,0.0,1.0);
 }else if(bTone.w>1.5&&bTone.w<2.5){c=vec3(dot(c,vec3(.2126,.7152,.0722)));
 }else if(bTone.w>2.5&&bTone.w<3.5){c=clamp((c-.5)*1.3+.5,0.0,1.0);c=cssSaturate(c,1.4);c=clamp(c*1.1,0.0,1.0);
 }else if(bTone.w>3.5&&bTone.w<4.5){c=clamp((c-.5)*1.1+.5,0.0,1.0);c=cssSaturate(c,1.2);
 }else if(bTone.w>4.5){c=clamp((c-.5)*1.05+.5,0.0,1.0);c=cssSaturate(c,1.1);c=clamp(c*1.02,0.0,1.0);}
 return c;
}
vec4 saturatePremultiplied(vec4 c,float s){return vec4(cssSaturate(unassociated(c),s)*c.a,c.a);}
