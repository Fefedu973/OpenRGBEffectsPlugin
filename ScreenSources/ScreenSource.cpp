/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ScreenSource.h"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

#if defined(_WIN32) && !defined(SCREEN_SOURCES_DISABLE_FRAME_SURFACE) && __has_include(<FrameSurface/FrameSurface.h>)
#include <FrameSurface/FrameSurface.h>
#define SCREEN_SOURCES_FRAME_SURFACE 1
#endif

namespace screen_source
{
using Clock = std::chrono::steady_clock;
const char* StateName(State state)
{
    switch(state)
    {
    case State::Starting:return "Starting"; case State::Live:return "Live";
    case State::Static:return "Static"; case State::Busy:return "Busy";
    case State::Stale:return "Stale"; case State::Unavailable:return "Unavailable";
    case State::Invalid:return "Invalid"; case State::Unsupported:return "Unsupported";
    case State::Stopped:return "Stopped";
    }
    return "Unknown";
}

bool Config::Valid() const
{
    return !channel.empty() && channel.size() <= 64 &&
        std::all_of(channel.begin(),channel.end(),[](unsigned char c)
        { return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_'; }) &&
        poll_ms >= 5 && poll_ms <= 1000 && ttl_ms >= 1 && ttl_ms <= 60000 &&
        max_width >= 1 && max_width <= 16384 && max_height >= 1 && max_height <= 16384;
}

bool Snapshot::Usable() const
{
    return (state==State::Live || state==State::Static || state==State::Busy) &&
        frame && !frame->image.isNull() && Clock::now() <= frame->expires;
}

std::shared_ptr<Source> Source::Acquire(const Config& config)
{
    if(!config.Valid())throw std::invalid_argument("Invalid screen source configuration");
    using Key=std::tuple<std::string,unsigned,unsigned,unsigned,unsigned>;
    static std::mutex registry_mutex;
    static std::map<Key,std::weak_ptr<Source>> registry;
    std::lock_guard<std::mutex> lock(registry_mutex);
    for(auto it=registry.begin();it!=registry.end();)
        if(it->second.expired())it=registry.erase(it);else ++it;
    const Key key{config.channel,config.poll_ms,config.ttl_ms,config.max_width,config.max_height};
    if(auto it=registry.find(key);it!=registry.end())
        if(auto source=it->second.lock())return source;
    if(registry.size()>=64)throw std::runtime_error("Too many active screen sources");
    auto source=std::shared_ptr<Source>(new Source(config));
    registry.emplace(key,source);
    return source;
}

Source::Source(const Config& value) : config(value)
{
#ifdef SCREEN_SOURCES_FRAME_SURFACE
    worker=std::thread(&Source::Run,this);
#else
    Publish(State::Unsupported,{},"FrameSurface input requires Windows and the OpenRGB Room header");
#endif
}

Source::~Source()
{
    {std::lock_guard<std::mutex> lock(stop_mutex);stopping=true;}
    wake.notify_all();
    if(worker.joinable())worker.join();
    Publish(State::Stopped,{},{});
}

Snapshot Source::Read() const
{
    std::lock_guard<std::mutex> lock(snapshot_mutex);
    auto value=snapshot;
    // Do not let a delayed worker keep an expired image alive in render code.
    if(value.frame && Clock::now()>value.frame->expires)
    {value.state=State::Stale;value.frame.reset();value.detail="Frame TTL expired";}
    return value;
}

void Source::Publish(State state,std::shared_ptr<const Frame> frame,std::string detail)
{
    std::lock_guard<std::mutex> lock(snapshot_mutex);
    if(snapshot.state==state && snapshot.frame==frame && snapshot.detail==detail)return;
    snapshot.state=state;snapshot.frame=std::move(frame);snapshot.detail=std::move(detail);++snapshot.revision;
}

#ifdef SCREEN_SOURCES_FRAME_SURFACE
static bool ValidPixels(const room_surface::Frame& frame,const Config& config)
{
    if(frame.width==0 || frame.height==0 || frame.width>config.max_width || frame.height>config.max_height ||
       frame.stride < std::uint64_t(frame.width)*4 || frame.stride > 0x7fffffffu ||
       std::uint64_t(frame.stride)*frame.height != frame.bgra.size() || frame.bgra.size()>room_surface::MAX_CAPACITY)
        return false;
    // The wire format promises opaque BGRA; fail closed on a malformed producer.
    for(unsigned y=0;y<frame.height;++y)for(unsigned x=0;x<frame.width;++x)
        if(frame.bgra[std::size_t(y)*frame.stride+std::size_t(x)*4+3]!=255)return false;
    return true;
}

static std::shared_ptr<const Frame> OwnFrame(room_surface::Frame& input,unsigned ttl)
{
    const auto now_ms=GetTickCount64();
    if(input.timestamp_ms>now_ms || now_ms-input.timestamp_ms>ttl)return {};
    auto result=std::make_shared<Frame>();
    result->sequence=input.sequence;result->generation=input.generation;result->timestamp_ms=input.timestamp_ms;
    result->received=Clock::now();
    result->expires=result->received+std::chrono::milliseconds(ttl-(now_ms-input.timestamp_ms));
    // Transfer the Reader's owned vector; Qt owns it until the final image alias
    // disappears. A later producer write cannot mutate it, and no extra full-image
    // copy is required. The const-data overload gives normal detach-on-edit semantics.
    auto pixels=std::make_unique<std::vector<std::uint8_t>>(std::move(input.bgra));
    result->image=QImage(static_cast<const uchar*>(pixels->data()),int(input.width),int(input.height),int(input.stride),
        QImage::Format_ARGB32,[](void* owner){delete static_cast<std::vector<std::uint8_t>*>(owner);},pixels.get());
    if(result->image.isNull())return {};
    pixels.release();
    return result;
}
#endif

void Source::Run()
{
#ifdef SCREEN_SOURCES_FRAME_SURFACE
    room_surface::Reader reader(config.channel);
    room_surface::Frame incoming;
    std::shared_ptr<const Frame> current;
    bool rejected=false;
    for(;;)
    {
        {std::lock_guard<std::mutex> lock(stop_mutex);if(stopping)break;}
        try
        {
            const auto state=reader.ReadLatest(incoming,config.ttl_ms,5);
            using S=room_surface::FrameStatus;
            switch(state)
            {
            case S::NewFrame:
                rejected=!ValidPixels(incoming,config);
                current=rejected?nullptr:OwnFrame(incoming,config.ttl_ms);
                Publish(rejected?State::Invalid:(current?State::Live:State::Stale),current,
                    rejected?"Invalid frame dimensions, stride or opaque BGRA pixels":(current?"":"Frame TTL expired"));
                break;
            case S::Unchanged:
                if(rejected)Publish(State::Invalid,{},"Invalid frame dimensions, stride or opaque BGRA pixels");
                else if(current && Clock::now()<=current->expires)Publish(State::Static,current,{});
                else Publish(State::Stale,{},"Frame TTL expired");
                break;
            case S::Busy:
                if(current && Clock::now()>current->expires){current.reset();Publish(State::Stale,{},"Frame TTL expired while producer is busy");}
                else Publish(State::Busy,current,reader.LastError());
                break;
            case S::Stale:
                current.reset();Publish(State::Stale,{},reader.LastError());break;
            case S::Unavailable:
                current.reset();rejected=false;Publish(State::Unavailable,{},reader.LastError());
                // Release an old mapping so a replacement can choose a new capacity.
                reader.Close();incoming.bgra.clear();break;
            case S::Invalid:
                current.reset();rejected=false;Publish(State::Invalid,{},reader.LastError());reader.Close();incoming.bgra.clear();break;
            }
        }
        catch(const std::exception&)
        {
            // Only aggregate state is exposed: never frame bytes, scene URLs or tokens.
            current.reset();rejected=false;reader.Close();incoming.bgra.clear();
            Publish(State::Invalid,{},"Screen source read/allocation failed");
        }
        std::unique_lock<std::mutex> lock(stop_mutex);
        if(wake.wait_for(lock,std::chrono::milliseconds(config.poll_ms),[this]{return stopping;}))break;
    }
#endif
}
}
