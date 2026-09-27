// SPDX-License-Identifier: GPL-2.0-or-later
void mainImage(out vec4 color,in vec2 p){
 vec2 uv=(logicalUV(p)-bPlacement.xy)/bPlacement.zw;
 if(bPixelated>.5){vec2 size=vec2(textureSize(iChannel0,0));uv=(floor(uv*size)+.5)/size;}
 color=image0(uv);
}
