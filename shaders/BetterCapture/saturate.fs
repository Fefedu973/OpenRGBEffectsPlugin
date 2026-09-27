// SPDX-License-Identifier: GPL-2.0-or-later
void mainImage(out vec4 color,in vec2 p){color=saturatePremultiplied(image0(logicalUV(p)),bGlow.x);}
