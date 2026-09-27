// SPDX-License-Identifier: GPL-2.0-or-later
// Original restrained composition, adapted from our MusicEngine prototype.
// Tempo/phase are observations from Effects' existing tracker, not an AI model.
#include "MusicDirector.h"
#include <mutex>

namespace room_ai
{
namespace
{
constexpr double Tau=6.2831853071795864769, Freshness=.150;
enum class Scene {Silence,Calm,Flow,Groove,Energetic};
const char* Name(Scene s)
{
    switch(s){case Scene::Calm:return "Calm";case Scene::Flow:return "Flow";
    case Scene::Groove:return "Groove";case Scene::Energetic:return "Energetic";default:return "Silence";}
}
Color Palette(Scene s)
{
    Color c;
    switch(s){case Scene::Calm:c={.94f,.57f,.28f};break;case Scene::Flow:c={.28f,.61f,.83f};break;
    case Scene::Groove:c={.25f,.83f,.64f};break;case Scene::Energetic:c={.65f,.35f,.90f};break;
    default:return {};}
    return {Decode(c.r),Decode(c.g),Decode(c.b)};
}
struct Accent {double time=-1e9,strength=0,x=0,y=0;};
struct Published
{
    MusicSnapshot snapshot;
    Scene from=Scene::Silence,to=Scene::Silence;
    double transition_time=0,intensity=.6,artistic_level=0;
    std::array<Accent,16> accents{};
};
bool Valid(const room_audio::RhythmSnapshot& r)
{
    if(!std::isfinite(r.audio_time)||!std::isfinite(r.last_onset_time)||
       !std::isfinite(r.power)||r.power<0||!std::isfinite(r.onset_strength)||
       !std::isfinite(r.confidence)||!std::isfinite(r.bpm)||!std::isfinite(r.phase))return false;
    for(float value:r.spectrum)if(!std::isfinite(value))return false;
    return true;
}
MusicSnapshot AtTime(const Published& p,double now)
{
    auto s=p.snapshot;
    const double age=now-s.time;
    if(!std::isfinite(now)||age<0||age>Freshness||!s.valid)
    {
        s.valid=false;s.beat=false;s.level=s.onset=s.tempo_bpm=s.phase=s.confidence=0;s.bands.fill(0);
        if(s.scene!="Silence")s.scene="Silence";
        if(age>Freshness)s.reason="Audio stale: rhythmic prediction disabled";
        else if(age<0||!std::isfinite(now))s.reason="Invalid analysis clock";
        return s;
    }
    s.onset*=std::exp(-age/.12);
    if(s.tempo_bpm>0){const double phase=s.phase+age*s.tempo_bpm/60;s.phase=phase-std::floor(phase);}
    s.beat=false;
    for(const auto& a:p.accents)if(now>=a.time&&now-a.time<.12)s.beat=true;
    return s;
}
}

struct MusicDirector::Impl
{
    mutable std::mutex publication;
    std::mutex writer;
    Published published;
    std::uint64_t sequence=0,generation=0,onset=0,accent_sequence=0;
    bool stream=false;
    double first_time=0,last_time=0,last_now=0,energy=0,onset_rate=0;
    Scene current=Scene::Silence,candidate=Scene::Silence,previous=Scene::Silence;
    double scene_start=0,candidate_since=0,transition_time=0;
    std::array<Accent,16> accents{};
    std::size_t accent_next=0;
    double last_accent=-1e9;

    void Clear(const std::string& reason)
    {
        stream=false;sequence=generation=onset=accent_sequence=0;
        first_time=last_time=last_now=energy=onset_rate=scene_start=candidate_since=transition_time=0;
        current=candidate=previous=Scene::Silence;accents.fill({});accent_next=0;last_accent=-1e9;
        std::lock_guard<std::mutex> lock(publication);const double intensity=published.intensity;
        published={};published.intensity=intensity;published.snapshot.reason=reason;
    }
    void Analyze(const room_audio::RhythmSnapshot& r,double now)
    {
        const bool first=!stream;
        const double t=r.audio_time,dt=first?.01:std::clamp(t-last_time,0.,Freshness);
        if(first){first_time=t;stream=true;onset=r.onset_sequence;}
        last_time=t;last_now=now;sequence=r.sequence;generation=r.generation;
        MusicSnapshot out;out.time=t;out.valid=true;
        out.level=std::clamp(std::sqrt(double(r.power)),0.,1.);
        const bool locked=r.locked&&r.confidence>=.5f&&r.bpm>=60&&r.bpm<=180;
        // Preserve the measured score without claiming a calibrated probability.
        out.confidence=std::clamp(double(r.confidence),0.,1.);
        out.tempo_bpm=locked?double(r.bpm):0;
        out.phase=locked?double(r.phase)-std::floor(double(r.phase)):0;
        const double onset_age=now-r.last_onset_time;
        const bool fresh_onset=r.last_onset_time>=0&&r.last_onset_time<=r.audio_time&&onset_age>=0&&onset_age<=Freshness;
        out.onset=fresh_onset?std::clamp(double(r.onset_strength),0.,1.):0;
        out.backend="Existing native multiband rhythm tracker; no AI model";
        constexpr unsigned edges[]={1,3,6,11,21,41,81,141,200};
        for(unsigned band=0;band<8;++band){float peak=0;for(unsigned i=edges[band];i<edges[band+1];++i)peak=std::max(peak,r.spectrum[i]);out.bands[band]=std::clamp(peak,0.f,1.f);}
        energy+=(1-std::exp(-dt/1.5))*(out.level-energy);
        onset_rate*=std::exp(-dt/2.);
        const auto difference=r.onset_sequence>=onset?r.onset_sequence-onset:0;
        const bool new_onset=!first&&difference!=0;
        onset=r.onset_sequence;
        // Lost render observations affect density only. Never fabricate the
        // omitted events' time/location, nor replay them as a burst of accents.
        if(new_onset&&fresh_onset)onset_rate+=.5*double(std::min<std::uint64_t>(difference,3));
        std::string reason;
        if(r.silent)
        {
            current=candidate=previous=Scene::Silence;scene_start=candidate_since=transition_time=t;
            accents.fill({});last_accent=-1e9;energy=onset_rate=0;
            out.level=out.onset=out.tempo_bpm=out.phase=out.confidence=0;out.bands.fill(0);
            reason="Measured silence: no periodic accents";
        }
        else
        {
            const Scene desired=locked?(onset_rate>3?Scene::Energetic:Scene::Groove):(onset_rate>1.5?Scene::Flow:Scene::Calm);
            if(current==Scene::Silence){current=previous=Scene::Calm;scene_start=transition_time=t;}
            if(desired!=candidate){candidate=desired;candidate_since=t;}
            if(candidate!=current&&t-scene_start>=8&&t-candidate_since>=2)
            {previous=current;current=candidate;transition_time=scene_start=t;reason="Scene transition: persistent rhythm/activity evidence";}
            if(reason.empty())reason=locked?"Stable pulse: restrained rhythmic motion":
                (t-first_time<3?"Warming up: levels and transients only":"Uncertain pulse: non-metrical motion");
            if(new_onset&&fresh_onset&&r.onset_strength>=.35f)
            {
                unsigned recent=0;for(const auto& a:accents)if(now>=a.time&&now-a.time<4)++recent;
                if(now-last_accent>=.28&&recent<4)
                {
                    const double phase=std::fmod(++accent_sequence*.6180339887498948,1.);
                    accents[accent_next]={r.last_onset_time,std::min(.35,double(r.onset_strength)*.35),phase,.18+.22*std::sin(Tau*phase)};
                    accent_next=(accent_next+1)%accents.size();last_accent=now;
                    reason="Selected transient accent: within four-per-four-second budget";
                }
                else reason="Transient ignored: accent density/refractory budget";
            }
        }
        out.scene=Name(current);out.reason=std::move(reason);
        std::lock_guard<std::mutex> lock(publication);
        published.snapshot=std::move(out);published.from=previous;published.to=current;
        published.transition_time=transition_time;published.accents=accents;published.artistic_level=energy;
    }
};

MusicDirector::MusicDirector():impl(new Impl){}
MusicDirector::~MusicDirector()=default;
void MusicDirector::Reset(){std::lock_guard<std::mutex> lock(impl->writer);impl->Clear("Waiting for audio");}
void MusicDirector::SetIntensity(double value)
{
    std::lock_guard<std::mutex> lock(impl->publication);
    impl->published.intensity=std::isfinite(value)?std::clamp(value,0.,1.):0;
}
void MusicDirector::Push(const room_audio::RhythmSnapshot& r,double now)
{
    std::lock_guard<std::mutex> lock(impl->writer);
    if(!std::isfinite(now)||!Valid(r)) {impl->Clear("Invalid analysis values/clock");return;}
    const double age=now-r.audio_time;
    if(!r.sequence||age<0||age>Freshness)
    {impl->Clear(age<0?"Audio clock is ahead of render clock":"Audio unavailable or stale");return;}
    if(impl->stream&&(r.generation!=impl->generation||r.sequence<impl->sequence||
       r.audio_time<impl->last_time||now<impl->last_now||now-impl->last_now>Freshness))
        impl->Clear("Audio discontinuity: composition reset");
    // PublishNoPacket changes silent without advancing sequence or audio_time.
    // It must take effect immediately, even if the renderer already saw this hop.
    if(impl->stream&&r.sequence==impl->sequence&&!r.silent){impl->last_now=now;return;}
    impl->Analyze(r,now);
}
MusicSnapshot MusicDirector::Snapshot(double now)const
{
    Published state;{std::lock_guard<std::mutex> lock(impl->publication);state=impl->published;}
    return AtTime(state,now);
}
MusicRenderState MusicDirector::RenderState(double now)const
{
    Published state;{std::lock_guard<std::mutex> lock(impl->publication);state=impl->published;}
    const auto s=AtTime(state,now);MusicRenderState out;
    if(!s.valid||s.scene=="Silence")return out;
    out.valid=true;out.bands=s.bands;
    const double fade=std::clamp((now-state.transition_time)/2.,0.,1.);
    const auto a=Palette(state.from),b=Palette(state.to);
    out.palette={float(a.r+(b.r-a.r)*fade),float(a.g+(b.g-a.g)*fade),float(a.b+(b.b-a.b)*fade)};
    const double energy=std::clamp(std::sqrt(state.artistic_level*3),0.,1.);
    out.controls={float(.035+.28*energy),float(state.intensity),float(s.phase),s.tempo_bpm>0?1.f:0.f};
    out.motion_phase=float(std::fmod(now*.7,Tau));
    for(std::size_t i=0;i<out.accents.size();++i)
    {
        const auto& accent=state.accents[i];const double age=now-accent.time;
        if(age>=0&&age<.8)out.accents[i]={float(accent.x),float(accent.y),float(accent.strength*std::exp(-age/.18)),0};
    }
    return out;
}
Color MusicDirector::Sample(Vec2 point,double now,bool directed)const
{
    if(!std::isfinite(point.x)||!std::isfinite(point.y))return {};
    const auto s=RenderState(now);if(!s.valid)return {};
    if(!directed)
    {
        const unsigned band=unsigned(std::clamp(std::floor(point.x*8),0.,7.));
        if(point.y<.5625*(1-std::sqrt(s.bands[band]))||point.y>.5625)return {};
        const float gain=s.controls[1]*.6f;return {gain*.12f,gain*.65f,gain};
    }
    double motion=.95+.05*std::sin(s.motion_phase+point.x*3-point.y*2);
    if(s.controls[3]>0)motion=.9+.1*std::cos(Tau*(s.controls[2]-point.x*.35));
    double light=s.controls[0]*motion;
    for(const auto& a:s.accents){const double dx=point.x-a[0],dy=point.y-a[1];light+=a[2]*std::exp(-(dx*dx+dy*dy)/.05);}
    const float gain=float(std::clamp(light*s.controls[1],0.,1.));
    return Clamp({s.palette.r*gain,s.palette.g*gain,s.palette.b*gain});
}
}
