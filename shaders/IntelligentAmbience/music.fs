// SPDX-License-Identifier: GPL-2.0-or-later
// Function fragment: root wrapper supplies mainImage. All output is LINEAR RGB.
// MusicDirector::RenderState packs these uniforms once per frame. No audio I/O.
uniform vec3 iaMusicPalette;
uniform vec4 iaMusicControls; // base level, intensity, beat phase, tempo enabled
uniform float iaMusicMotionPhase;
uniform vec4 iaMusicAccents[16]; // world x/y, decayed amplitude, reserved
uniform float iaMusicDirected;
uniform vec4 iaMusicBands0;
uniform vec4 iaMusicBands1;
// iaScreenHeight is declared by the accompanying video fragment.

vec3 iaMusicSample(vec2 world)
{
    if(iaMusicDirected<0.5)
    {
        int band=int(clamp(floor(world.x*8.0),0.0,7.0));
        float level=band<4?iaMusicBands0[band]:iaMusicBands1[band-4];
        float height=max(0.0,iaScreenHeight);
        if(world.y<height*(1.0-sqrt(clamp(level,0.0,1.0)))||world.y>height)
            return vec3(0);
        return vec3(0.12,0.65,1.0)*iaMusicControls.y*0.6;
    }
    float motion=0.95+0.05*sin(iaMusicMotionPhase+world.x*3.0-world.y*2.0);
    if(iaMusicControls.w>0.5)
        motion=0.9+0.1*cos(6.28318530718*(iaMusicControls.z-world.x*0.35));
    float light=iaMusicControls.x*motion;
    for(int i=0;i<16;++i)
    {
        vec2 delta=world-iaMusicAccents[i].xy;
        light+=iaMusicAccents[i].z*exp(-dot(delta,delta)/0.05);
    }
    return clamp(iaMusicPalette*clamp(light*iaMusicControls.y,0.0,1.0),0.0,1.0);
}
