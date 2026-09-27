// SPDX-License-Identifier: GPL-2.0-or-later
// Persistent native framebuffer, with the four reference HSL targets.
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    float index=mod(floor(t_speed*0.03),4.0);
    float hue=322.0,lightness=0.35;
    if(index>2.5){hue=202.0;lightness=0.35;}
    else if(index>1.5){hue=230.0;lightness=0.26;}
    else if(index>0.5){hue=280.0;lightness=0.25;}
    vec3 target=HSVToRGB(vec3(hue/360.0,1.0,2.0*lightness));
    vec4 previous=texture2D(iPreviousFrame,fragCoord/iResolution.xy);
    vec3 background=previous.a>0.5?previous.rgb:vec3(0.0);
    fragColor=vec4(mix(background,target,0.05),1.0);
}
