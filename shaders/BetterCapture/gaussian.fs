// SPDX-License-Identifier: GPL-2.0-or-later
// Separable Gaussian, bounded 65 taps. Large logical sigmas use weighted linear
// samples; this is not a promise of browser-kernel bit identity.
void mainImage(out vec4 color,in vec2 p){
 vec2 uv=logicalUV(p);float sigma=bKernel.x;
 if(sigma<.01){color=image0(uv);return;}
 float radius=ceil(3.0*sigma),steps=min(32.0,radius),stride=radius/max(1.0,steps);
 vec2 direction=bKernel.yz/iResolution.xy;vec4 sum=vec4(0.0);float weightSum=0.0;
 for(int i=-32;i<=32;i++){
  if(abs(float(i))>steps)continue;float x=float(i)*stride,w=exp(-.5*x*x/(sigma*sigma));
  sum+=image0(uv+direction*x)*w;weightSum+=w;
 }
 color=sum/max(weightSum,.00001);
}
