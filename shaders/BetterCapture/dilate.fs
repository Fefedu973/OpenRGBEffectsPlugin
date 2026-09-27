// SPDX-License-Identifier: GPL-2.0-or-later
// Flat rectangular channel-wise dilation. Each stage expands a contiguous
// interval by <= its current width, giving exact integer-radius support in
// logarithmically many nine-tap stages rather than a radius-squared loop.
void mainImage(out vec4 color,in vec2 p){
 vec2 uv=logicalUV(p);color=vec4(0.0);
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)
  color=max(color,image0(uv+vec2(float(x),float(y))*bMorphStep/iResolution.xy));
}
