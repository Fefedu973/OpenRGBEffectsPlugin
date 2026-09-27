// SPDX-License-Identifier: GPL-2.0-or-later
#include "VideoEngine.h"
#include <chrono>
#include <limits>
#include <numeric>

namespace room_ai
{
namespace
{
constexpr unsigned GridSide=64;
constexpr std::size_t MaxPixels=8u*1024u*1024u, MaxEvents=128;
constexpr double MaxFrameAge=.75, MaxMotionGap=.30, MaxDistance=1.25;
float Luma(Color c){return .2126f*c.r+.7152f*c.g+.0722f*c.b;}
Color Add(Color a,Color b){return {a.r+b.r,a.g+b.g,a.b+b.b};}
Color Mul(Color a,double b){return {float(a.r*b),float(a.g*b),float(a.b*b)};}
double Distance(Vec2 a,Vec2 b){return std::hypot(a.x-b.x,a.y-b.y);}
double ColorDistance(Color a,Color b){return (std::abs(a.r-b.r)+std::abs(a.g-b.g)+std::abs(a.b-b.b))/3.;}
struct Cell {Color color;float gray=0;};
struct Grid
{
    unsigned w=0,h=0;
    std::vector<Cell> cells;
    const Cell& At(int x,int y)const{return cells[std::size_t(std::clamp(y,0,int(h)-1))*w+std::clamp(x,0,int(w)-1)];}
};
Grid Reduce(const VideoFrame& f)
{
    Grid g;
    const double s=std::min(1.,double(GridSide)/std::max(f.width,f.height));
    g.w=std::max(1u,unsigned(std::lround(f.width*s)));
    g.h=std::max(1u,unsigned(std::lround(f.height*s)));
    g.cells.resize(std::size_t(g.w)*g.h);
    static const std::array<float,256> linear=[](){std::array<float,256>a{};for(unsigned i=0;i<256;++i)a[i]=Decode(i/255.f);return a;}();
    // Bounded stratified area approximation, <= 16 input reads per cell. Decode
    // each sRGB sample BEFORE averaging, rather than averaging encoded bytes.
    for(unsigned y=0;y<g.h;++y)for(unsigned x=0;x<g.w;++x)
    {
        const unsigned nx=std::min(4u,std::max(1u,f.width/g.w));
        const unsigned ny=std::min(4u,std::max(1u,f.height/g.h));
        Color c{};
        for(unsigned j=0;j<ny;++j)for(unsigned i=0;i<nx;++i)
        {
            const unsigned sx=std::min(f.width-1,unsigned((x+(i+.5)/nx)*f.width/g.w));
            const unsigned sy=std::min(f.height-1,unsigned((y+(j+.5)/ny)*f.height/g.h));
            const auto n=(std::size_t(sy)*f.width+sx)*3;
            c=Add(c,{linear[(*f.rgb)[n]],linear[(*f.rgb)[n+1]],linear[(*f.rgb)[n+2]]});
        }
        c=Mul(c,1./(nx*ny));g.cells[std::size_t(y)*g.w+x]={c,Luma(c)};
    }
    return g;
}
double GridDifference(const Grid&a,const Grid&b,double*changed=nullptr)
{
    double sum=0,n=0;
    for(std::size_t i=0;i<a.cells.size();++i){const auto d=ColorDistance(a.cells[i].color,b.cells[i].color);sum+=d;if(d>.24)++n;}
    if(changed)*changed=n/a.cells.size();
    return sum/a.cells.size();
}
bool BrightUniformFlash(const Grid& g)
{
    double sum=0,low=1;
    for(const auto&c:g.cells){sum+=c.gray;low=std::min(low,double(c.gray));}
    return sum/g.cells.size()>.88 && low>.72;
}
double PatchError(const Grid&a,const Grid&b,int x,int y,int dx,int dy)
{
    double error=0;
    for(int py=-2;py<=2;++py)for(int px=-2;px<=2;++px)
        error+=std::abs(a.At(x+px,y+py).gray-b.At(x+px-dx,y+py-dy).gray);
    return error/25.;
}
struct Flow {int x=0,y=0,dx=0,dy=0;double quality=0;};
std::vector<Flow> Match(const Grid& current,const Grid& previous)
{
    std::vector<Flow> result;result.reserve(256);
    if(current.w<7||current.h<7)return result;
    for(int y=2;y<int(current.h)-2;y+=3)for(int x=2;x<int(current.w)-2;x+=3)
    {
        float low=1,high=0;
        for(int j=-2;j<=2;++j)for(int i=-2;i<=2;++i){const auto l=current.At(x+i,y+j).gray;low=std::min(low,l);high=std::max(high,l);}
        if(high-low<.045f)continue; // no direction can be inferred in a flat patch
        double best=PatchError(current,previous,x,y,0,0), zero=best;
        int bestx=0,besty=0;
        for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx)
        {
            if(x-dx<2||x-dx>=int(previous.w)-2||y-dy<2||y-dy>=int(previous.h)-2)continue;
            const double e=PatchError(current,previous,x,y,dx,dy);
            // Prefer the smallest displacement at an equal error (aperture/ties).
            if(e<best-1e-7||(std::abs(e-best)<=1e-7&&dx*dx+dy*dy<bestx*bestx+besty*besty))
            {best=e;bestx=dx;besty=dy;}
        }
        if(bestx==0&&besty==0)
        {
            if(best<.03)result.push_back({x,y,0,0,std::clamp(1.-best/.03,0.,1.)});
            continue;
        }
        const double improvement=zero-best;
        if(improvement<.008||best>.12)continue;
        const double q=std::clamp(improvement/(zero+.01),0.,1.)*std::clamp(1.-best/.12,0.,1.);
        if(q>.18)result.push_back({x,y,bestx,besty,q});
    }
    return result;
}
struct Candidate {Vec2 position,velocity;Color color;double weight=0,quality=0;int edge=0;};
struct Event {Vec2 position,velocity;Color color;double seen=0,born=0,sigma=.04,quality=0;int edge=0;};
}

struct VideoEngine::Impl
{
    Grid current,cut_anchor;
    Stamp stamp{};
    unsigned source_w=0,source_h=0;
    double height=.5625,persistence=.8,strength=.7;
    double alive_time=0;
    bool valid=false,pending_cut=false,flash=false;
    std::vector<Event> events;
    VideoStats stats;
    std::uint64_t revision=1;
    mutable std::shared_ptr<const VideoRenderState> render_state;
    void Invalidate(){if(++revision==0)++revision;render_state.reset();}
    void Clear()
    {
        current={};cut_anchor={};stamp={};source_w=source_h=0;alive_time=0;
        valid=pending_cut=flash=false;events.clear();stats={};
        Invalidate();
    }
    double Outside(Vec2 p)const
    {return Distance(p,{std::clamp(p.x,0.,1.),std::clamp(p.y,0.,height)});}
    Color Base(Vec2 p)const
    {
        const double x=std::clamp(p.x,0.,1.)*current.w-.5;
        const double y=std::clamp(p.y/height,0.,1.)*current.h-.5;
        const int ix=int(std::floor(x)),iy=int(std::floor(y));
        const double fx=x-ix,fy=y-iy;
        return Add(Add(Mul(current.At(ix,iy).color,(1-fx)*(1-fy)),Mul(current.At(ix+1,iy).color,fx*(1-fy))),
                   Add(Mul(current.At(ix,iy+1).color,(1-fx)*fy),Mul(current.At(ix+1,iy+1).color,fx*fy)));
    }
    void Advance(double dt,double now)
    {
        for(auto&e:events){e.position.x+=e.velocity.x*dt;e.position.y+=e.velocity.y*dt;}
        events.erase(std::remove_if(events.begin(),events.end(),[&](const Event&e){return now-e.seen>3*persistence||now-e.born>6*persistence||Outside(e.position)>MaxDistance;}),events.end());
    }
    void ObserveMotion(const std::vector<Flow>& flow,double dt,double now)
    {
        // Median global translation is only reported with spatially distributed
        // support. Keep it in local trajectories: a camera pan also transports
        // visible colors beyond the screen. Never blindly subtract that motion.
        if(flow.size()>=12)
        {
            std::vector<int> xs,ys;xs.reserve(flow.size());ys.reserve(flow.size());
            std::array<bool,4> quadrants{};
            for(const auto&f:flow)
            {xs.push_back(f.dx);ys.push_back(f.dy);quadrants[(f.x>=int(current.w/2)?1:0)+(f.y>=int(current.h/2)?2:0)]=true;}
            std::sort(xs.begin(),xs.end());std::sort(ys.begin(),ys.end());
            const int dx=xs[xs.size()/2],dy=ys[ys.size()/2];
            unsigned inliers=0;for(const auto&f:flow)if(std::abs(f.dx-dx)<=1&&std::abs(f.dy-dy)<=1)++inliers;
            const double support=double(inliers)/flow.size();
            if(std::count(quadrants.begin(),quadrants.end(),true)>=3&&support>=.6)
            {stats.camera_velocity={dx/(current.w*dt),dy*height/(current.h*dt)};stats.camera_confidence=support;}
        }
        // Four bins per edge, fixed independent of the number/placement of LEDs.
        std::array<Candidate,16> bins{};
        for(const auto&f:flow)
        {
            Vec2 p{(f.x+.5)/current.w,(f.y+.5)*height/current.h};
            Vec2 v{f.dx/(current.w*dt),f.dy*height/(current.h*dt)};
            const double speed=std::hypot(v.x,v.y);
            if(speed<.015||speed>8.)continue;
            int edge=-1;double outward=0;
            auto choose=[&](bool within,double component,int id){if(within&&component>outward){outward=component;edge=id;}};
            choose(p.x<.16,-v.x,0);choose(p.x>.84,v.x,1);
            choose(p.y<height*.16,-v.y,2);choose(p.y>height*.84,v.y,3);
            if(edge<0||outward<.02)continue;
            Color color{};double weight=0;
            for(int j=-2;j<=2;++j)for(int i=-2;i<=2;++i)
            {
                const auto& c=current.At(f.x+i,f.y+j);const double w=c.gray*c.gray;
                color=Add(color,Mul(c.color,w));weight+=w;
            }
            if(weight<.01)continue;
            color=Mul(color,1./weight);
            const int segment=std::clamp(int((edge<2?p.y/height:p.x)*4),0,3);
            auto&bin=bins[edge*4+segment];
            const double w=f.quality*std::min(1.,weight/2.);
            bin.position.x+=p.x*w;bin.position.y+=p.y*w;
            bin.velocity.x+=v.x*w;bin.velocity.y+=v.y*w;
            bin.color=Add(bin.color,Mul(color,w));bin.quality+=f.quality*w;bin.weight+=w;bin.edge=edge;
        }
        std::vector<bool> used(events.size(),false);
        double quality_sum=0;unsigned count=0;
        for(auto&b:bins)
        {
            if(b.weight<=0)continue;
            b.position.x/=b.weight;b.position.y/=b.weight;b.velocity.x/=b.weight;b.velocity.y/=b.weight;
            b.color=Mul(b.color,1./b.weight);b.quality/=b.weight;
            std::size_t match=events.size();double distance=.13;
            for(std::size_t i=0;i<events.size();++i)
            {
                if(used[i]||events[i].edge!=b.edge||ColorDistance(events[i].color,b.color)>.30)continue;
                const double d=Distance(events[i].position,b.position);
                if(d<distance){distance=d;match=i;}
            }
            if(match<events.size())
            {
                auto&e=events[match];used[match]=true;e.position=b.position;
                e.velocity={.35*e.velocity.x+.65*b.velocity.x,.35*e.velocity.y+.65*b.velocity.y};
                e.color=b.color;e.seen=now;e.quality=b.quality;
            }
            else if(events.size()<MaxEvents)
            {
                events.push_back({b.position,b.velocity,b.color,now,now,.045,b.quality,b.edge});used.push_back(true);
            }
            quality_sum+=b.quality;++count;
        }
        stats.confidence=count?quality_sum/count:0;
    }
};

VideoEngine::VideoEngine():impl(new Impl){}
VideoEngine::~VideoEngine()=default;
VideoEngine::VideoEngine(VideoEngine&&) noexcept=default;
VideoEngine&VideoEngine::operator=(VideoEngine&&) noexcept=default;
void VideoEngine::Reset(){impl->Clear();}
void VideoEngine::SetPersistence(double s){if(std::isfinite(s)){s=std::clamp(s,.1,5.);if(s!=impl->persistence){impl->persistence=s;impl->Invalidate();}}}
void VideoEngine::SetStrength(double s){if(std::isfinite(s)){s=std::clamp(s,0.,1.);if(s!=impl->strength){impl->strength=s;impl->Invalidate();}}}
void VideoEngine::SetScreenHeight(double h)
{
    if(!std::isfinite(h)||h<.1||h>4.||h==impl->height)return;
    impl->Clear();impl->height=h;
}
VideoStats VideoEngine::Stats()const{return impl->stats;}
std::shared_ptr<const VideoRenderState> VideoEngine::RenderState()const
{
    auto&p=*impl;if(p.render_state)return p.render_state;
    auto state=std::make_shared<VideoRenderState>();
    state->revision=p.revision;state->event_count=p.events.size();state->source_time=p.stamp.source_time;state->alive_time=p.alive_time;
    state->screen_height=p.height;state->persistence=p.persistence;state->strength=p.strength;
    state->valid=p.valid;state->suppress_prediction=p.pending_cut||p.flash;
    state->grid.width=std::max(1u,p.current.w);state->grid.height=std::max(1u,p.current.h);
    auto grid=std::make_shared<std::vector<float>>(std::size_t(state->grid.width)*state->grid.height*4,0.f);
    for(std::size_t i=0;i<p.current.cells.size();++i)
    {const auto c=p.current.cells[i].color;(*grid)[4*i]=c.r;(*grid)[4*i+1]=c.g;(*grid)[4*i+2]=c.b;(*grid)[4*i+3]=1.f;}
    state->grid.rgba=grid;
    state->events.width=3;state->events.height=unsigned(MaxEvents);
    auto events=std::make_shared<std::vector<float>>(MaxEvents*12,0.f);
    for(std::size_t i=0;i<p.events.size();++i)
    {
        const auto&e=p.events[i];const auto j=i*12;
        (*events)[j]=float(e.position.x);(*events)[j+1]=float(e.position.y);
        (*events)[j+2]=float(e.velocity.x);(*events)[j+3]=float(e.velocity.y);
        (*events)[j+4]=e.color.r;(*events)[j+5]=e.color.g;(*events)[j+6]=e.color.b;(*events)[j+7]=float(e.quality);
        (*events)[j+8]=float(p.stamp.source_time-e.seen);(*events)[j+9]=float(p.stamp.source_time-e.born);(*events)[j+10]=float(e.sigma);
    }
    state->events.rgba=events;
    p.render_state=state;return state;
}
bool VideoEngine::Refresh(double now)
{
    auto&p=*impl;
    if(!p.valid||!std::isfinite(now)||now<p.alive_time||now<p.stamp.source_time)return false;
    if(now==p.alive_time)return true;
    p.alive_time=now;
    if(p.render_state)
    {
        auto next=std::make_shared<VideoRenderState>(*p.render_state);
        next->alive_time=now;p.render_state=next;
    }
    return true;
}
bool VideoEngine::Push(const VideoFrame& f)
{
    auto&p=*impl;const auto start=std::chrono::steady_clock::now();
    auto reject=[&](){++p.stats.rejected_frames;return false;};
    if(!f.rgb||!f.width||!f.height||f.width>8192||f.height>8192||
       std::size_t(f.width)*f.height>MaxPixels||f.rgb->size()!=std::size_t(f.width)*f.height*3||!std::isfinite(f.stamp.source_time))return reject();
    const bool same_epoch=p.valid&&f.stamp.stream_epoch==p.stamp.stream_epoch&&f.stamp.state_epoch==p.stamp.state_epoch;
    if(p.valid&&(f.stamp.stream_epoch<p.stamp.stream_epoch||
       (f.stamp.stream_epoch==p.stamp.stream_epoch&&f.stamp.state_epoch<p.stamp.state_epoch)))return reject();
    if(same_epoch&&(f.stamp.sequence<=p.stamp.sequence||f.stamp.source_time<=p.stamp.source_time))return reject();
    const double dt=p.valid?f.stamp.source_time-p.stamp.source_time:0;
    const bool discontinuity=!same_epoch||f.width!=p.source_w||f.height!=p.source_h||dt>MaxMotionGap||dt<1e-6;
    Grid next=Reduce(f);
    p.stats.cut=false;p.stats.confidence=0;p.stats.camera_velocity={};p.stats.camera_confidence=0;
    if(discontinuity)
    {
        p.events.clear();p.pending_cut=p.flash=false;p.cut_anchor={};
        p.stats.mode=p.valid?"Classical / discontinuity reset":"Classical / first frame";
    }
    else
    {
        p.Advance(dt,f.stamp.source_time);
        double changed=0;const double difference=GridDifference(next,p.current,&changed);
        bool suppress=false;
        if(p.pending_cut)
        {
            // A one-frame flash returning to its original scene preserves memory.
            const bool returned=GridDifference(next,p.cut_anchor)<.08;
            if(!returned){p.events.clear();p.stats.cut=true;}
            p.pending_cut=p.flash=false;p.cut_anchor={};suppress=true;
            p.stats.mode=returned?"Classical / transient flash":"Classical / confirmed cut";
        }
        else if(difference>.20&&changed>.72)
        {
            // Do not interpret a full-screen transient as a field of moving lights.
            p.cut_anchor=p.current;p.pending_cut=true;
            if(BrightUniformFlash(next)){p.flash=true;p.stats.mode="Classical / possible flash";}
            else{p.stats.mode="Classical / possible cut";}
            suppress=true;
        }
        if(!suppress)
        {
            const auto flows=Match(next,p.current);
            p.current=std::move(next);
            p.ObserveMotion(flows,dt,f.stamp.source_time);
            p.stats.mode="Procedural motion memory";
        }
    }
    if(!next.cells.empty())p.current=std::move(next);
    p.stamp=f.stamp;p.alive_time=f.stamp.source_time;p.source_w=f.width;p.source_h=f.height;p.valid=true;
    p.stats.event_count=p.events.size();
    p.stats.frame_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    p.Invalidate();
    return true;
}

Color VideoEngine::Sample(Vec2 point,double now,bool predictive)const
{
    const auto&p=*impl;
    if(!p.valid||!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(now)||
       now<p.stamp.source_time||now<p.alive_time||now-p.alive_time>MaxFrameAge)return {};
    Color color=p.Base(point);
    if(!predictive||p.strength==0||p.pending_cut||p.flash||p.Outside(point)==0)return color;
    if(p.Outside(point)>MaxDistance)return color;
    const double dt=now-p.stamp.source_time;
    for(const auto&e:p.events)
    {
        const double age=now-e.seen;
        if(age>3*p.persistence||now-e.born>6*p.persistence)continue;
        const Vec2 center{e.position.x+e.velocity.x*dt,e.position.y+e.velocity.y*dt};
        if(p.Outside(center)>MaxDistance)continue;
        const double sigma2=e.sigma*e.sigma+.002*std::max(0.,age);
        const double distance=Distance(point,center);
        const double gain=p.strength*e.quality*std::exp(-age/p.persistence-distance*distance/(2*sigma2)) *
                          std::exp(-p.Outside(center)/MaxDistance);
        color=Add(color,Mul(e.color,gain));
    }
    return Clamp(color);
}
std::vector<Color> VideoEngine::Sample(const std::vector<Vec2>&points,double now,bool predictive)const
{
    std::vector<Color> result;result.reserve(points.size());
    for(const auto point:points)result.push_back(Sample(point,now,predictive));
    return result;
}
}
