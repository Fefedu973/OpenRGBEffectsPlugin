// SPDX-License-Identifier: GPL-2.0-or-later
uniform vec4 prState;
const float spStep=0.10471975511966;
vec2 spPoint(float n,float gap,float angle){float t=n*spStep;return t*gap*vec2(cos(t+angle),sin(t+angle));}
vec2 spNormal(vec2 v){return vec2(-v.y,v.x);}
float spCross(vec2 a,vec2 b){return a.x*b.y-a.y*b.x;}
float spEdge(vec2 p,vec2 a,vec2 b){vec2 v=b-a;return length(p-a-v*clamp(dot(p-a,v)/max(dot(v,v),.000001),0.0,1.0));}
float spSegment(vec2 p,vec2 a,vec2 b,float width,bool start,bool end)
{
    vec2 v=b-a;float size=length(v),t=dot(p-a,v)/size,side=abs(spCross(v,p-a))/size;
    float distance=length(vec2(max(max(-t,t-size),0.0),max(side-width,0.0)));
    if(t>=0.0&&t<=size&&side<=width)
    {
        float depth=width-side;if(start)depth=min(depth,t);if(end)depth=min(depth,size-t);return -depth;
    }
    return distance;
}
float spJoin(vec2 p,vec2 center,vec2 incoming,vec2 outgoing,float width)
{
    float turn=spCross(incoming,outgoing);if(abs(turn)<.000001)return 10000.0;
    float sign=turn>0.0?-1.0:1.0;
    vec2 a=center+sign*spNormal(incoming)*width,b=center+sign*spNormal(outgoing)*width;
    vec2 normal=normalize(sign*(spNormal(incoming)+spNormal(outgoing)));
    vec2 m=center+normal*(width/dot(normal,sign*spNormal(outgoing)));
    vec4 sides=vec4(spCross(a-center,p-center),spCross(m-a,p-a),spCross(b-m,p-m),spCross(center-b,p-b));
    float outer=min(spEdge(p,a,m),spEdge(p,m,b));
    if(all(greaterThanEqual(sides,vec4(0)))||all(lessThanEqual(sides,vec4(0))))return -outer;
    return min(outer,min(spEdge(p,b,center),spEdge(p,center,a)));
}
vec3 spHue(float h){return clamp(abs(fract(h+vec3(0,2.0/3.0,1.0/3.0))*6.0-3.0)-1.0,0.0,1.0);}
void mainImage(out vec4 color,in vec2 position)
{
    vec2 point=vec2(position.x/iResolution.x,1.0-position.y/iResolution.y)*vec2(320,200)-vec2(160,100);
    float angle=p_speed>0.0?prState.x/(150.0-p_speed):0.0;
    if(p_reverse>.5)angle=-angle;
    float halfWidth=p_size*.5,aa=max(.15,160.0/iResolution.x),radius=length(point);
    // A conservative radial bound selects exact polyline segments. No curve
    // approximation or unbounded per-pixel search is needed for the 599 segments.
    float padding=2.0*halfWidth+spStep*p_lineGap+1.0;
    int first=int(clamp(floor((radius-padding)/(spStep*p_lineGap))-1.0,0.0,598.0));
    int last=int(clamp(ceil((radius+padding)/(spStep*p_lineGap))+1.0,0.0,598.0));
    float distance=10000.0;
    for(int i=first;i<=last;++i)
    {
        float n=float(i);vec2 a=spPoint(n,p_lineGap,angle),b=spPoint(n+1.0,p_lineGap,angle);
        distance=min(distance,spSegment(point,a,b,halfWidth,i==0,i==598));
        if(i<598)distance=min(distance,spJoin(point,b,normalize(b-a),normalize(spPoint(n+2.0,p_lineGap,angle)-b),halfWidth));
    }
    vec3 ink=p_rainbow>.5?spHue(floor(angle*(p_colorSpeed+2.0)+.5)/360.0):p_fg;
    color=vec4(mix(p_bg,ink,1.0-smoothstep(-aa,aa,distance)),1.0);
}
