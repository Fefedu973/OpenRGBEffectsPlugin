/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BetterDiscovery.h"
#include "BetterControlState.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <set>
#include <deque>

namespace better_source
{
namespace
{
constexpr qint64 DescriptorLimit=16384,ResponseLimit=262144;
const QString Root=QStringLiteral("/api/native/v1/");
bool Guid(const QString& v)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return re.match(v).hasMatch()&&v!=QStringLiteral("00000000-0000-0000-0000-000000000000");
}
bool UInt(const QJsonValue& value,unsigned lo,unsigned hi,unsigned& out)
{
    if(!value.isDouble())return false;double d=value.toDouble();
    if(!std::isfinite(d)||d!=std::floor(d)||d<lo||d>hi)return false;out=unsigned(d);return true;
}
bool Json(const QByteArray& bytes,QJsonObject& out)
{
    QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;out=doc.object();return true;
}
struct Descriptor { QUrl base;QByteArray token;QString instance;unsigned pid=0; };
State Load(const QString& path,Descriptor& d)
{
    QFile file(path);
    if(!QFileInfo::exists(path))return State::MissingDescriptor;
    if(!file.open(QIODevice::ReadOnly)||file.size()<2||file.size()>DescriptorLimit)return State::InvalidDescriptor;
    auto bytes=file.read(DescriptorLimit+1);QJsonObject j;if(bytes.size()>DescriptorLimit||!Json(bytes,j))return State::InvalidDescriptor;
    static const QRegularExpression base(QStringLiteral("^http://127\\.0\\.0\\.1:([1-9][0-9]{0,4})$"));
    static const QRegularExpression token(QStringLiteral("^[0-9a-fA-F]{64}$"));
    const auto match=base.match(j.value("baseUrl").toString());unsigned port=match.hasMatch()?match.captured(1).toUInt():0;
    unsigned api=0;
    if(!UInt(j.value("apiVersion"),1,1,api)||!match.hasMatch()||port==0||port>65535||
       j.value("authorizationScheme").toString()!=QStringLiteral("Bearer")||!token.match(j.value("token").toString()).hasMatch()||
       j.value("discoveryPath").toString()!=Root+"discovery"||!Guid(j.value("instanceId").toString())||
       !UInt(j.value("processId"),1,0xffffffffu,d.pid))return State::InvalidDescriptor;
    d.base=QUrl(j.value("baseUrl").toString(),QUrl::StrictMode);d.token=j.value("token").toString().toLatin1();d.instance=j.value("instanceId").toString().toLower();
    return d.base.isValid()?State::Ready:State::InvalidDescriptor;
}
bool DiscoveryData(const QJsonObject& j,const Descriptor& descriptor,Snapshot& s)
{
    auto transport=j.value("transport").toObject();unsigned version=0,header=0,capacity=0,publisher=0,ttl=0;
    unsigned api=0;
    if(!UInt(j.value("apiVersion"),1,1,api)||j.value("application").toString()!="BetterSignalRGBScreenCapture"||
       j.value("instanceId").toString().toLower()!=descriptor.instance||transport.value("name").toString()!="ORGBFRM1"||
       !UInt(transport.value("version"),1,1,version)||!UInt(transport.value("headerBytes"),128,128,header)||
       !UInt(transport.value("maxCapacity"),4,67108864,capacity)||!UInt(transport.value("publisherCapacity"),4,capacity,publisher)||
       !UInt(transport.value("recommendedTtlMs"),100,60000,ttl)||transport.value("scope").toString()!="windows-current-user-session")return false;
    auto canvas=j.value("canvas").toObject();if(!UInt(canvas.value("width"),1,4096,s.canvas_width)||!UInt(canvas.value("height"),1,4096,s.canvas_height))return false;
    auto capabilities=j.value("capabilities");if(!capabilities.isArray()||capabilities.toArray().size()>64)return false;
    for(const auto& cap:capabilities.toArray()){if(!cap.isString()||cap.toString().size()>128)return false;s.capabilities.push_back(cap.toString());}
    if(!s.capabilities.contains("raw-composite")||!s.capabilities.contains("opacity-coverage")||!s.capabilities.contains("saved-scenes"))return false;
    auto outputs=j.value("outputs");if(!outputs.isArray()||outputs.toArray().size()!=2)return false;
    static const QRegularExpression channel(QStringLiteral("^BetterCapture-[0-9a-fA-F]{24}-(Raw|Coverage)$"));
    for(const auto& item:outputs.toArray())
    {
        auto o=item.toObject();auto name=o.value("channel").toString();
        if(!channel.match(name).hasMatch()||o.value("format").toString()!="BGRA8_OPAQUE_SRGB")return false;
        if(o.value("id").toString()=="canvas-raw"&&o.value("role").toString()=="opaque-black-composite"&&name.endsWith("-Raw")&&s.raw_channel.isEmpty())s.raw_channel=name;
        else if(o.value("id").toString()=="canvas-coverage"&&o.value("role").toString()=="opacity-coverage-grayscale"&&name.endsWith("-Coverage")&&s.coverage_channel.isEmpty())s.coverage_channel=name;
        else return false;
    }
    if(s.raw_channel.isEmpty()||s.coverage_channel.isEmpty()||s.raw_channel.left(s.raw_channel.size()-4)!=s.coverage_channel.left(s.coverage_channel.size()-9))return false;
    s.instance_id=descriptor.instance;s.process_id=descriptor.pid;s.recommended_ttl_ms=ttl;return true;
}
bool SceneData(const QJsonObject& j,Snapshot& s)
{
    unsigned api=0;
    if(!UInt(j.value("apiVersion"),1,1,api)||!j.value("scenes").isArray()||j.value("scenes").toArray().size()>4096)return false;
    QStringList ids;
    for(const auto& item:j.value("scenes").toArray())
    {
        auto scene=item.toObject();QString id=scene.value("id").toString().toLower(),name=scene.value("name").toString();
        if(!Guid(id)||!scene.value("name").isString()||name.size()>1024||ids.contains(id))return false;
        ids.push_back(id);s.scenes.push_back({id,name});
    }
    return true;
}
}
const char* StateName(State state)
{
    switch(state){case State::Starting:return "starting";case State::Ready:return "ready";case State::MissingDescriptor:return "missing_descriptor";case State::InvalidDescriptor:return "invalid_descriptor";case State::Unavailable:return "unavailable";case State::Unauthorized:return "unauthorized";case State::Incompatible:return "incompatible";case State::Stopped:return "stopped";}return "invalid";
}
class Discovery::Impl final:public QThread
{
public:
    explicit Impl(Config c):config(std::move(c)),snapshot(std::make_shared<const Snapshot>()),control(std::make_shared<const ControlSnapshot>()){start();}
    ~Impl() override{stopping=true;wake.notify_all();wait();}
    std::shared_ptr<const Snapshot> Read()const{std::lock_guard<std::mutex> lock(snapshot_mutex);return snapshot;}
    void Refresh(){requested=true;wake.notify_all();}
    void RequestState(std::uint64_t generation)
    {
        if(!generation)return;const auto instance=Read()->instance_id;
        std::lock_guard<std::mutex> lock(state_mutex);const auto found=states.find(generation);
        if(found!=states.end())
        {
            if(found->second.value->status==PublishedStateStatus::Ready)return;
            if(found->second.value->status==PublishedStateStatus::Pending&&(state_requested==generation||state_inflight==generation))return;
            if(Clock::now()-found->second.at<std::chrono::milliseconds(250))return;
        }
        const auto previous=state_requested.exchange(generation);
        if(previous&&previous!=generation){auto old=states.find(previous);if(old!=states.end()){auto v=*old->second.value;v.status=PublishedStateStatus::Unavailable;v.detail="request_superseded";old->second.value=std::make_shared<const PublishedState>(v);}}
        PublishedState v;v.instance_id=instance;v.requested_generation=generation;PutState(std::move(v));wake.notify_all();
    }
    std::shared_ptr<const PublishedState> ReadState(std::uint64_t generation)const
    {std::lock_guard<std::mutex> lock(state_mutex);auto found=states.find(generation);return found==states.end()?nullptr:found->second.value;}
    RequestResult RequestScene(const QString& owner,const QString& scene)
    {
        if(owner.isEmpty()||owner.size()>128)return RequestResult::Invalid;
        for(auto ch:owner)if(ch.isNull()||ch.unicode()<32||ch.unicode()==127)return RequestResult::Invalid;
        if(scene.isEmpty()){ReleaseScene(owner);return RequestResult::Accepted;}
        if(!Guid(scene))return RequestResult::Invalid;
        std::lock_guard<std::mutex> lock(control_mutex);
        auto result=claims.Claim(owner.toStdString(),scene.toLower().toStdString());
        if(result==SceneClaims::ClaimResult::Conflict||result==SceneClaims::ClaimResult::Full)
        {if(denied.size()<64)denied.insert(owner);return RequestResult::Conflict;}
        if(result==SceneClaims::ClaimResult::Invalid)return RequestResult::Invalid;
        owners.insert(owner);denied.erase(owner);
        if(result!=SceneClaims::ClaimResult::Unchanged){control_requested=true;wake.notify_all();}
        return RequestResult::Accepted;
    }
    void ReleaseScene(const QString& owner)
    {
        std::lock_guard<std::mutex> lock(control_mutex);owners.erase(owner);denied.erase(owner);
        if(claims.Release(owner.toStdString())!=SceneClaims::ReleaseResult::Unknown){control_requested=true;wake.notify_all();}
    }
    std::shared_ptr<const ControlSnapshot> ReadControl(const QString& owner)const
    {
        std::lock_guard<std::mutex> lock(control_mutex);ControlSnapshot result;
        if(denied.count(owner)){result.phase=ControlPhase::Conflict;result.detail="local_scene_conflict";return std::make_shared<const ControlSnapshot>(result);}
        if(!owners.count(owner))return std::make_shared<const ControlSnapshot>(result);
        const auto scene=QString::fromStdString(claims.Scene());
        if(control_epoch!=claims.Epoch()||control->scene_id!=scene){result.phase=ControlPhase::Waiting;result.detail="waiting";result.scene_id=scene;return std::make_shared<const ControlSnapshot>(result);}
        return control;
    }
private:
    using Clock=std::chrono::steady_clock;
    struct Response { State state=State::Unavailable;int status=0;QJsonObject body; };
    struct Intent { std::uint64_t epoch=0;QString scene; };
    const Config config;std::atomic<bool> stopping{false},requested{false},control_requested{false};std::mutex sleep_mutex;
    std::condition_variable wake;mutable std::mutex snapshot_mutex,control_mutex;
    std::shared_ptr<const Snapshot> snapshot;std::shared_ptr<const ControlSnapshot> control;
    struct CacheEntry{std::shared_ptr<const PublishedState> value;Clock::time_point at;};
    mutable std::mutex state_mutex;std::map<std::uint64_t,CacheEntry> states;std::deque<std::uint64_t> state_order;
    std::atomic<std::uint64_t> state_requested{0},state_inflight{0};
    void PutState(PublishedState value)
    {
        const auto gen=value.requested_generation;
        state_order.erase(std::remove(state_order.begin(),state_order.end(),gen),state_order.end());state_order.push_back(gen);
        states[gen]={std::make_shared<const PublishedState>(std::move(value)),Clock::now()};
        while(state_order.size()>8){states.erase(state_order.front());state_order.pop_front();}
    }
    void ClearStates(){std::lock_guard<std::mutex> lock(state_mutex);states.clear();state_order.clear();}
    SceneClaims claims;std::set<QString> owners,denied;std::uint64_t control_epoch=0;
    QString lease_id,lease_scene;Descriptor lease_descriptor;SceneTarget target;
    bool pending=false;std::uint64_t blocked_epoch=~std::uint64_t(0);
    Clock::time_point renewed{},pending_since{};
    Intent Desired()const{std::lock_guard<std::mutex> lock(control_mutex);return {claims.Epoch(),QString::fromStdString(claims.Scene())};}
    void Publish(Snapshot value){std::lock_guard<std::mutex> lock(snapshot_mutex);value.revision=snapshot->revision+1;snapshot=std::make_shared<const Snapshot>(std::move(value));}
    void PublishControl(ControlSnapshot value,std::uint64_t epoch)
    {std::lock_guard<std::mutex> lock(control_mutex);if(claims.Epoch()!=epoch)return;value.revision=control->revision+1;control_epoch=epoch;control=std::make_shared<const ControlSnapshot>(std::move(value));}
    void ControlStatus(ControlPhase phase,const char* detail,const Intent& intent)
    {ControlSnapshot v;v.phase=phase;v.detail=detail;v.scene_id=intent.scene;v.instance_id=lease_descriptor.instance;PublishControl(std::move(v),intent.epoch);}
    Response Request(QNetworkAccessManager& manager,const Descriptor& d,const QByteArray& method,const QString& path,const QJsonObject& body={},bool cleanup=false)
    {
        QUrl url=d.base;url.setPath(path);QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);
        request.setRawHeader("Authorization",QByteArray("Bearer ")+d.token);request.setRawHeader("Accept","application/json");
        QByteArray payload;if(!body.isEmpty()){payload=QJsonDocument(body).toJson(QJsonDocument::Compact);request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");}
        auto* reply=manager.sendCustomRequest(request,method,payload);reply->setReadBufferSize(ResponseLimit+1);
        QEventLoop loop;QTimer poll,deadline;poll.setInterval(20);deadline.setSingleShot(true);QByteArray bytes;bool oversized=false,timed_out=false;
        auto consume=[&]{bytes+=reply->read(ResponseLimit+1-bytes.size());if(bytes.size()>ResponseLimit){oversized=true;reply->abort();}};
        QObject::connect(reply,&QNetworkReply::readyRead,&loop,consume);QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
        QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(stopping&&!cleanup){reply->abort();loop.quit();}});
        QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timed_out=true;reply->abort();loop.quit();});
        poll.start();deadline.start(cleanup?750:(method=="GET"?config.request_timeout_ms:std::max(3000u,config.request_timeout_ms)));
        if(!reply->isFinished())loop.exec();consume();poll.stop();deadline.stop();
        Response result;result.status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto error=reply->error();delete reply;
        if(stopping&&!cleanup){result.state=State::Stopped;return result;}
        if(oversized){result.state=State::Incompatible;return result;}if(timed_out)return result;
        if(result.status==401||result.status==403){result.state=State::Unauthorized;return result;}
        // Qt reports HTTP4xx as network errors. The bounded JSON error code is
        // still parsed for lease policy; 3xx is never followed or accepted.
        if((result.status<200||result.status>=300)&&result.status!=409&&result.status!=404)return result;
        if(error!=QNetworkReply::NoError&&result.status<400)return result;
        result.state=Json(bytes,result.body)?State::Ready:State::Incompatible;return result;
    }
    State Get(QNetworkAccessManager& manager,const Descriptor& d,const QString& path,QJsonObject& json)
    {auto r=Request(manager,d,"GET",path);if(r.state==State::Ready&&r.status!=200)return State::Unavailable;json=std::move(r.body);return r.state;}
    static bool Revision(const QJsonValue& value,std::uint64_t& result,bool zero=false)
    {
        if(!value.isDouble())return false;double v=value.toDouble();
        if(!std::isfinite(v)||v<std::uint64_t(zero?0:1)||v>9007199254740991.0||v!=std::floor(v))return false;result=std::uint64_t(v);return true;
    }
    static bool Effective(const QJsonObject& value,EffectiveScene& e,std::uint64_t& generation,std::uint64_t& sequence)
    {
        auto scene=value.value("scene").toObject(),image=value.value("image").toObject();
        e.scene_id=scene.value("activeSceneId").toString().toLower().toStdString();e.scene_loading=scene.value("sceneLoading").toBool(true);
        return Revision(value.value("controlRevision"),e.control_revision)&&Revision(scene.value("stateRevision"),e.scene_revision,true)&&
            ParseDecimal64(image.value("generation").toString().toStdString(),generation)&&ParseDecimal64(image.value("sequence").toString().toStdString(),sequence)&&
            image.value("outputId").toString()=="canvas-raw"&&image.value("format").toString()=="BGRA8_OPAQUE_SRGB";
    }
    void ForgetLease(){lease_id.clear();lease_scene.clear();pending=false;target={};}
    bool Release(QNetworkAccessManager& manager,bool cleanup=false)
    {
        if(lease_id.isEmpty())return true;
        auto r=Request(manager,lease_descriptor,"DELETE",Root+"leases/"+lease_id,{},cleanup);
        const QString error=r.body.value("error").toString();
        if((r.state==State::Ready&&(r.status==200||r.status==202||error=="lease_expired"||error=="lease_conflict"))||Clock::now()-renewed>std::chrono::seconds(31))
        {ForgetLease();return true;}return false;
    }
    void HandleFailure(const Response& r,const Intent& intent,bool acquiring)
    {
        const QString code=r.body.value("error").toString();
        if(r.state==State::Ready&&(code=="command_in_progress"||code=="scene_busy")){ControlStatus(ControlPhase::Waiting,"command_busy",intent);return;}
        blocked_epoch=intent.epoch;
        if(code=="state_superseded"||code=="manual_override"){ControlStatus(ControlPhase::Superseded,"state_superseded",intent);return;}
        if(code=="lease_conflict"){ControlStatus(ControlPhase::Conflict,"lease_conflict",intent);return;}
        // A failed/aborted acquisition may have created ownership without
        // returning its capability. Never immediately reacquire in a loop.
        ControlStatus(ControlPhase::Unavailable,acquiring?"acquire_uncertain":"control_unavailable",intent);
    }
    bool AcceptTarget(const Response& r,const Descriptor& d,const Intent& intent,bool acquiring,bool renewal=false)
    {
        if(r.state!=State::Ready||(r.status!=200&&r.status!=202)){HandleFailure(r,intent,acquiring);return false;}
        const auto state=renewal?r.body:r.body.value("control").toObject();
        if(acquiring)
        {
            const auto id=state.value("lease").toObject().value("id").toString();
            if(!Guid(id)){blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"missing_lease_capability",intent);return false;}
            lease_id=id;lease_descriptor=d;lease_scene=intent.scene;renewed=Clock::now();
        }
        const auto scene=state.value("scene").toObject();std::uint64_t revision=0,scene_revision=0;
        const bool valid=Revision(state.value("revision"),revision)&&Revision(scene.value("stateRevision"),scene_revision,true)&&scene.value("activeSceneId").toString().compare(intent.scene,Qt::CaseInsensitive)==0;
        if(!valid){blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_control_ack",intent);return false;}
        if(!renewal)
        {
            std::uint64_t requested_revision=0,requested_scene=0;
            if(!Revision(r.body.value("targetControlRevision"),requested_revision)||!Revision(r.body.value("targetSceneRevision"),requested_scene,true)||requested_revision!=revision||requested_scene!=scene_revision)
            {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_control_target",intent);return false;}
        }
        target={d.instance.toStdString(),revision,scene_revision,intent.scene.toStdString()};lease_scene=intent.scene;if(acquiring||renewal)renewed=Clock::now();pending_since=Clock::now();pending=true;
        ControlStatus(ControlPhase::Pending,"pending_frame",intent);
        if(!renewal&&r.status==200&&r.body.value("result").toString()=="effective")Observe(r.body.value("effectiveState").toObject(),revision,d,intent);
        return true;
    }
    void Observe(const QJsonObject& value,std::uint64_t current,const Descriptor& d,const Intent& intent)
    {
        EffectiveScene effective;std::uint64_t generation=0,sequence=0;
        bool valid=Effective(value,effective,generation,sequence);
        auto result=ObserveScene(target,d.instance.toStdString(),current,valid?&effective:nullptr);
        if(result==SceneResult::Superseded||result==SceneResult::InstanceChanged)
        {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Superseded,"state_superseded",intent);return;}
        if(result!=SceneResult::Effective){ControlStatus(ControlPhase::Pending,"pending_frame",intent);return;}
        // Bind the result to the discovered raw output as well as the exact
        // control/scene revision. The reader still validates TTL and dimensions.
        if(value.value("image").toObject().value("channel").toString()!=Read()->raw_channel)
        {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_effective_channel",intent);return;}
        pending=false;if(target.scene_id!=intent.scene.toStdString()){ControlStatus(ControlPhase::Waiting,"scene_transition",intent);return;}ControlSnapshot v;v.phase=ControlPhase::Effective;v.detail="effective";v.scene_id=intent.scene;v.instance_id=d.instance;
        v.control_revision=target.control_revision;v.scene_revision=target.scene_revision;v.effective_generation=generation;v.minimum_sequence=sequence;PublishControl(std::move(v),intent.epoch);
    }
    void ProcessState(QNetworkAccessManager& manager,const Descriptor& d,bool available)
    {
        const auto generation=state_requested.exchange(0);if(!generation)return;state_inflight=generation;
        PublishedState v;v.requested_generation=generation;v.instance_id=d.instance;v.status=PublishedStateStatus::Unavailable;v.detail="state_unavailable";
        if(available)
        {
            auto r=Request(manager,d,"GET",Root+"states/"+QString::number(qulonglong(generation)));
            if(r.state==State::Ready&&r.status==200)
            {
                std::uint64_t actual=0;const auto image=r.body.value("image").toObject();
                if(ParseDecimal64(image.value("generation").toString().toStdString(),actual)&&actual==generation&&image.value("channel").toString()==Read()->raw_channel)
                {v.status=PublishedStateStatus::Ready;v.detail="ready";v.envelope=std::move(r.body);}
                else{v.status=PublishedStateStatus::Invalid;v.detail="invalid_state_generation";}
            }
            else if(r.state==State::Incompatible){v.status=PublishedStateStatus::Invalid;v.detail="invalid_state_json";}
        }
        std::lock_guard<std::mutex> lock(state_mutex);PutState(std::move(v));state_inflight=0;
    }
    void ProcessControl(QNetworkAccessManager& manager,const Descriptor& d,bool available)
    {
        const auto intent=Desired();
        if(intent.scene.isEmpty()){if(!lease_id.isEmpty()){ControlStatus(ControlPhase::Releasing,"releasing",intent);if(!Release(manager))return;}ControlStatus(ControlPhase::Idle,"idle",intent);return;}
        if(!lease_id.isEmpty()&&available&&lease_descriptor.instance!=d.instance){ForgetLease();blocked_epoch=~std::uint64_t(0);}
        if(blocked_epoch==intent.epoch){if(!lease_id.isEmpty())Release(manager);return;}
        if(!available){ControlStatus(ControlPhase::Waiting,"discovery_unavailable",intent);return;}
        if(lease_id.isEmpty())
        {
            auto response=Request(manager,d,"POST",Root+"leases",{{"clientId","openrgb-room/effects"},{"ttlSeconds",30},{"sceneId",intent.scene}});
            AcceptTarget(response,d,intent,true);return;
        }
        if(pending&&Clock::now()-pending_since>std::chrono::seconds(25))
        {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"pending_frame_timeout",intent);Release(manager);return;}
        if(!pending&&Clock::now()-renewed>=std::chrono::seconds(10))
        {
            // Renew does not select a different scene. Finish its effective
            // frame before applying a newly requested scene on this lease.
            Intent renewing=intent;renewing.scene=lease_scene;
            auto response=Request(manager,d,"PUT",Root+"leases/"+lease_id+"/renew",{{"ttlSeconds",30}});AcceptTarget(response,d,renewing,false,true);return;
        }
        if(!pending&&lease_scene!=intent.scene)
        {
            auto response=Request(manager,d,"PUT",Root+"leases/"+lease_id+"/scene",{{"sceneId",intent.scene}});AcceptTarget(response,d,intent,false);return;
        }
        QJsonObject status;auto result=Get(manager,d,Root+"status",status);std::uint64_t current=0;
        if(result!=State::Ready||!Revision(status.value("control").toObject().value("revision"),current))
        {ControlStatus(ControlPhase::Unavailable,"status_unavailable",intent);return;}
        Observe(status.value("output").toObject().value("effectiveState").toObject(),current,d,intent);
    }
    void run()override
    {
        QNetworkAccessManager manager;manager.setProxy(QNetworkProxy::NoProxy);Descriptor d;bool available=false;auto next_discovery=Clock::time_point{};
        while(!stopping)
        {
            control_requested=false;
            if(requested.exchange(false)||Clock::now()>=next_discovery)
            {
                Descriptor fresh;Snapshot next;State state=Load(config.descriptor_path,fresh);
                if(state==State::Ready)
                {
                    QJsonObject discovery,scenes;state=Get(manager,fresh,Root+"discovery",discovery);
                    if(state==State::Ready&&!DiscoveryData(discovery,fresh,next))state=State::Incompatible;
                    if(state==State::Ready)state=Get(manager,fresh,Root+"scenes",scenes);
                    if(state==State::Ready&&!SceneData(scenes,next))state=State::Incompatible;
                }
                available=state==State::Ready;if(available){if(d.instance!=fresh.instance){ClearStates();blocked_epoch=~std::uint64_t(0);}d=fresh;}else next=Snapshot{};
                next.state=state;next.detail=QString::fromLatin1(StateName(state));Publish(std::move(next));next_discovery=Clock::now()+std::chrono::milliseconds(config.refresh_ms);
            }
            if(!stopping)ProcessControl(manager,d,available);
            if(!stopping)ProcessState(manager,d,available);
            auto deadline=next_discovery;if(!Desired().scene.isEmpty()||!lease_id.isEmpty())deadline=std::min(deadline,Clock::now()+std::chrono::milliseconds(250));
            std::unique_lock<std::mutex> lock(sleep_mutex);wake.wait_until(lock,deadline,[&]{return stopping||requested||control_requested||state_requested!=0;});
        }
        // Best effort release is bounded and uses only an owned capability.
        // If acquisition was interrupted before returning it, server TTL wins.
        Release(manager,true);Snapshot end;end.state=State::Stopped;end.detail="stopped";Publish(std::move(end));
    }
};
QString Discovery::DefaultDescriptorPath()
{
    const auto base=qEnvironmentVariable("LOCALAPPDATA");if(base.isEmpty())return {};
    return QDir(base).filePath("Better_SignalRGB_Screen_Capture/ApplicationData/NativeOutput/connection.json");
}
std::shared_ptr<Discovery> Discovery::Acquire(const Config& original)
{
    Config c=original;if(c.descriptor_path.isEmpty())c.descriptor_path=DefaultDescriptorPath();
    if(!QCoreApplication::instance()||!QFileInfo(c.descriptor_path).isAbsolute()||c.descriptor_path.size()>32767||
       c.refresh_ms<50||c.refresh_ms>60000||c.request_timeout_ms<100||c.request_timeout_ms>10000)throw std::invalid_argument("Invalid Better discovery configuration");
    c.descriptor_path=QDir::cleanPath(QFileInfo(c.descriptor_path).absoluteFilePath());
    const QString key=c.descriptor_path+"|"+QString::number(c.refresh_ms)+"|"+QString::number(c.request_timeout_ms);
    static std::mutex registry_mutex;static std::map<QString,std::weak_ptr<Discovery>> registry;
    std::lock_guard<std::mutex> lock(registry_mutex);
    for(auto i=registry.begin();i!=registry.end();)if(i->second.expired())i=registry.erase(i);else ++i;
    if(auto found=registry[key].lock())return found;
    auto value=std::shared_ptr<Discovery>(new Discovery(c));registry[key]=value;return value;
}
Discovery::Discovery(const Config& c):impl(new Impl(c)){}
Discovery::~Discovery()=default;
std::shared_ptr<const Snapshot> Discovery::Read()const{return impl->Read();}
void Discovery::Refresh(){impl->Refresh();}
void Discovery::RequestState(std::uint64_t generation){impl->RequestState(generation);}
std::shared_ptr<const PublishedState> Discovery::ReadState(std::uint64_t generation)const{return impl->ReadState(generation);}
RequestResult Discovery::RequestScene(const QString& owner,const QString& scene){return impl->RequestScene(owner,scene);}
void Discovery::ReleaseScene(const QString& owner){impl->ReleaseScene(owner);}
std::shared_ptr<const ControlSnapshot> Discovery::ReadControl(const QString& owner)const{return impl->ReadControl(owner);}
}
