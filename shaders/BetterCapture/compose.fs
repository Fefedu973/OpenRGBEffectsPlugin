// SPDX-License-Identifier: GPL-2.0-or-later
void mainImage(out vec4 color,in vec2 p){
 vec2 uv=logicalUV(p);vec4 picture=image0(uv),halo=vec4(0.0);
 if(bOptions.y>.5){
  if(bOptions.z>.5&&bOptions.w>.5)picture=vec4(0.0);
  if(bGlow.w>1.5)halo=image1(uv);
  else{
   vec2 glowUV=(uv*vec2(320,200)+vec2(bDomain.x))/bDomain.yz;
   halo=image1(glowUV);if(bGlow.w>.5)halo=.65*halo+.35*image2(glowUV);
   halo.rgb=clamp(unassociated(halo)*bGlow.y,0.0,1.0)*halo.a;
  }
 }
 color=vec4(clamp(picture.rgb+halo.rgb*(1.0-picture.a),0.0,1.0),1.0);
}
