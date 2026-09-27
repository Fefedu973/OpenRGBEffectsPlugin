// SPDX-License-Identifier: GPL-2.0-or-later
void mainImage(out vec4 color,in vec2 p){
 vec2 uv=logicalUV(p);
 if(bPixelated>.5){vec2 size=vec2(textureSize(iChannel0,0));uv=(floor(uv*size)+.5)/size;}
 float a=image1(uv).r;vec3 raw=image0(uv).rgb;
 color=vec4(a>0.00001?tone(clamp(raw/a,0.0,1.0))*a:vec3(0.0),a);
}
