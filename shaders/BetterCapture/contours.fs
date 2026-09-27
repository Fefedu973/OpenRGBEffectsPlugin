// SPDX-License-Identifier: GPL-2.0-or-later
vec4 fieldCell(vec2 cell,float halfIndex){
 cell=clamp(cell,vec2(0.0),bFieldSize.xy-1.0);
 return image2(vec2((cell.x+.5)/bFieldSize.x,(cell.y+.5+halfIndex*bFieldSize.y)/(2.0*bFieldSize.y)));
}
float maskAlpha(vec2 uv){
 if(outsideUV(uv))return 0.0;
 vec2 pos=uv*bFieldSize.xy-.5,cell=floor(pos),f=fract(pos);
 return mix(mix(fieldCell(cell,0.0).z,fieldCell(cell+vec2(1,0),0.0).z,f.x),
            mix(fieldCell(cell+vec2(0,1),0.0).z,fieldCell(cell+vec2(1,1),0.0).z,f.x),f.y);
}
void mainImage(out vec4 color,in vec2 p){
 vec2 uv=logicalUV(p),cell=floor(uv*bFieldSize.xy);vec4 field=fieldCell(cell,0.0);
 color=vec4(0.0);float outside=1.0-maskAlpha(uv);if(outside<.004||field.w<.5)return;
 vec2 q=field.xy,delta=(uv-q)*bFieldSize.zw;float distance=length(delta);if(distance>=bContour.z)return;
 vec2 cellDelta=((cell+.5)/bFieldSize.xy-q)*bFieldSize.zw;float d=length(cellDelta);
 vec2 inward=d>.01?-cellDelta/d:vec2(0,1),tangent=vec2(-inward.y,inward.x);
 vec3 limits=fieldCell(cell,1.0).rgb*255.0;vec3 sum=vec3(0.0);float alpha=0.0,samples=0.0;
 for(int side=-1;side<=1;side++){
  if(bContour.y<.001&&side!=0)continue;
  float limit=side<0?limits.x:(side==0?limits.y:limits.z);if(limit<.5)continue;
  float depth=max(0.0,(limit-1.0)/254.0)*bContour.x;
  for(int band=0;band<4;band++){
   vec2 point=q+(inward*(float(band)*depth/3.0)+tangent*float(side)*bContour.y)/bFieldSize.zw;
   if(outsideUV(point)||maskAlpha(point)<.05)break;
   vec2 source=clamp((point-bPlacement.xy)/bPlacement.zw,0.0,1.0);
   float a=image1(source).r;vec3 rgb=a>.00001?clamp(image0(source).rgb/a,0.0,1.0):vec3(0.0);
   float contribution=a*(bGlow.z>0.0?clamp(5.0*(dot(rgb,vec3(.2126,.7152,.0722))-bGlow.z),0.0,1.0):1.0);
   sum+=rgb*contribution;alpha+=contribution;samples+=1.0;
  }
 }
 if(alpha<.0001||samples<.5)return;
 vec3 rgb=sum/alpha;float l=dot(rgb,vec3(.2126,.7152,.0722));
 rgb=clamp(mix(vec3(l),rgb,bGlow.x)*bGlow.y,0.0,1.0);
 float fade=bContour.w<.0001?1.0:pow(clamp(1.0-distance/bContour.z,0.0,1.0),4.0*bContour.w);
 float a=alpha/samples*outside*fade;color=vec4(tone(rgb)*a,a);
}
