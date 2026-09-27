// SPDX-License-Identifier: GPL-2.0-or-later
// Integrated speed prevents discontinuities when the slider is moved or stopped.
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{fragColor=vec4(HSVToRGB(vec3(fract(t_nSpeed/240.0),1.0,1.0)),1.0);}
