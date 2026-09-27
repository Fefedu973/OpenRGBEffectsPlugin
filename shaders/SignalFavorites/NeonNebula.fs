// SPDX-License-Identifier: GPL-2.0-or-later
// Original GPU composition: persistent waves, falling squares, rising rings.
// Reference design coordinates are 320 x 200, origin at the top left.
// iPreviousFrame is a separate texture maintained by the optional feedback pass.
float neonHash(float v) { return fract(sin(v*127.1+311.7)*43758.5453); }
vec3 neonParticleColor(float index)
{
    if(index<1.0) return p_color4;
    if(index<2.0) return p_color5;
    if(index<3.0) return p_color6;
    if(index<4.0) return p_color1;
    if(index<5.0) return p_color2;
    return p_color3;
}
float neonLine(vec2 p,vec2 a,vec2 b)
{
    vec2 v=b-a;
    float u=clamp(dot(p-a,v)/max(dot(v,v),0.0001),0.0,1.0);
    return length(p-a-v*u);
}
void mainImage(out vec4 fragColor,in vec2 fragCoord)
{
    vec2 uv=fragCoord/iResolution.xy;
    vec2 p=vec2(uv.x,1.0-uv.y)*vec2(320.0,200.0);
    vec4 previous=texture2D(iPreviousFrame,uv);
    // The installed design paints this initial teal once, independently of
    // the editable second color; subsequent draws never erase the history.
    vec3 color=previous.a>0.5 ? previous.rgb : vec3(0.0,81.0,89.0)/255.0;
    float aa=max(0.35,320.0/iResolution.x);
    float stepPhase=mod(1001.0-24.0*t_speedRaw,1000.0);
    vec3 c0=p_color2,c1=p_color3,c2=p_color1;
    if(stepPhase>666.0) {c0=p_color1;c1=p_color2;c2=p_color3;}
    else if(stepPhase>333.0 && stepPhase<666.0) {c0=p_color3;c1=p_color1;c2=p_color2;}
    for(int wave=0;wave<3;++wave)
    {
        float baseline=float(wave)*100.0;
        float angle=(p.x+stepPhase)/20.0;
        float distanceCurve=abs(p.y-(baseline+40.0*sin(angle)))/sqrt(1.0+4.0*cos(angle)*cos(angle));
        if(p.x<4.0 || p.x>319.0) distanceCurve=1000.0;
        float firstY=baseline+40.0*sin((4.0+stepPhase)/20.0);
        float d=min(distanceCurve,neonLine(p,vec2(4.0,50.0),vec2(4.0,firstY)));
        float cover=1.0-smoothstep(2.5-aa,2.5+aa,d);
        vec3 stroke=wave==0?c0:(wave==1?c1:c2);
        // Two source-over strokes with alpha 0x25.
        color=mix(color,stroke,cover*(1.0-pow(1.0-37.0/255.0,2.0)));
    }
    // A fixed bounded population replaces mutable random object lists. Birth
    // phases and trajectories remain deterministic across frames and controls.
    for(int n=0;n<300;++n)
    {
        float seed=float(n)+1.0;
        bool rising=n>=150;
        float birthY=rising ? 300.0+100.0*neonHash(seed+2.0) : -100.0*neonHash(seed+2.0);
        float span=rising ? birthY : 250.0-birthY;
        float vy=0.5+2.0*neonHash(seed+3.0);
        float travel=mod(neonHash(seed+4.0)*span+12.0*t_speedRaw*vy,span);
        float y=birthY+(rising?-travel:travel);
        float x=320.0*neonHash(seed+5.0)+(2.0*neonHash(seed+6.0)-1.0)*travel/vy;
        vec2 delta=p-vec2(x,y);
        if(abs(delta.x)>11.0 || abs(delta.y)>11.0) continue;
        float radius=rising?2.5:5.0;
        float width=5.0+5.0*neonHash(seed+7.0);
        float d=rising ? abs(length(delta)-radius)-width*0.5 : max(abs(delta.x),abs(delta.y))-radius;
        float cover=1.0-smoothstep(-aa,aa,d);
        float alpha=rising?136.0/255.0:85.0/255.0;
        color=mix(color,neonParticleColor(floor(6.0*neonHash(seed+8.0))),cover*alpha);
    }
    if(p_tapEffect>0.5)
    {
        for(int tap=0;tap<64;++tap)
        {
            if(float(tap)>=iTapCount) break;
            float radius=6.0+120.0*max(0.0,iTapMeta[tap].y);
            if(radius>505.0) continue;
            float d=abs(length(p-iTapEvents[tap].xy)-radius);
            float cover=(1.0-smoothstep(2.5-aa,2.5+aa,d))*clamp(iTapEvents[tap].w,0.0,1.0);
            color=mix(color,p_color2,cover);
        }
    }
    fragColor=vec4(clamp(color,0.0,1.0),1.0);
}
