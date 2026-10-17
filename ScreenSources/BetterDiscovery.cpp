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
bool ValidOwner(const QString& owner)
{
    if(owner.isEmpty()||owner.size()>128)return false;
    for(auto ch:owner)if(ch.isNull()||ch.unicode()<32||ch.unicode()==127)return false;
    return true;
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
        if(!ValidOwner(owner))return RequestResult::Invalid;
        if(scene.isEmpty()){ReleaseScene(owner);return RequestResult::Accepted;}
        if(!Guid(scene))return RequestResult::Invalid;
        std::lock_guard<std::mutex> lock(control_mutex);
        const auto previous=DesiredUnlocked();
        auto result=claims.Claim(owner.toStdString(),scene.toLower().toStdString());
        if(result==SceneClaims::ClaimResult::Conflict||result==SceneClaims::ClaimResult::Full)
        {if(denied.size()<64)denied.insert(owner);return RequestResult::Conflict;}
        if(result==SceneClaims::ClaimResult::Invalid)return RequestResult::Invalid;
        owners.insert(owner);denied.erase(owner);
        IntentChanged(previous);
        return RequestResult::Accepted;
    }
    void ReleaseScene(const QString& owner)
    {
        std::lock_guard<std::mutex> lock(control_mutex);const auto previous=DesiredUnlocked();
        owners.erase(owner);denied.erase(owner);claims.Release(owner.toStdString());IntentChanged(previous);
    }
    RequestResult RequestCapture(const QString& owner,bool enabled)
    {
        if(!ValidOwner(owner))return RequestResult::Invalid;
        std::lock_guard<std::mutex> lock(control_mutex);const auto previous=DesiredUnlocked();
        if(enabled)
        {
            if(denied.count(owner)||(!capture_owners.count(owner)&&capture_owners.size()>=64))return RequestResult::Conflict;
            if(capture_owners.insert(owner).second)++capture_epoch;
        }
        else capture_owners.erase(owner);
        IntentChanged(previous);return RequestResult::Accepted;
    }
    void ReleaseOwner(const QString& owner)
    {
        std::lock_guard<std::mutex> lock(control_mutex);const auto previous=DesiredUnlocked();
        owners.erase(owner);denied.erase(owner);capture_owners.erase(owner);claims.Release(owner.toStdString());IntentChanged(previous);
    }
    std::shared_ptr<const ControlSnapshot> ReadControl(const QString& owner)const
    {
        std::lock_guard<std::mutex> lock(control_mutex);ControlSnapshot result;
        if(denied.count(owner)){result.phase=ControlPhase::Conflict;result.detail="local_scene_conflict";return std::make_shared<const ControlSnapshot>(result);}
        if(!owners.count(owner)&&!capture_owners.count(owner))return std::make_shared<const ControlSnapshot>(result);
        if(capture_owners.count(owner)&&revoked_capture_epoch&&capture_epoch<=revoked_capture_epoch)
        {result.phase=ControlPhase::Superseded;result.detail="capture_overridden";return std::make_shared<const ControlSnapshot>(result);}
        if(owners.count(owner)&&!superseded_scene.isEmpty()&&superseded_scene==QString::fromStdString(claims.Scene()))
        {result=*control;result.phase=ControlPhase::Superseded;result.detail="scene_overridden";return std::make_shared<const ControlSnapshot>(result);}
        const auto scene=QString::fromStdString(claims.Scene());
        if(control_epoch!=intent_epoch){result.phase=ControlPhase::Waiting;result.detail="waiting";result.scene_id=scene;return std::make_shared<const ControlSnapshot>(result);}
        if(!owners.count(owner)&&capture_owners.count(owner)&&control->capture_active)
        {result=*control;result.phase=ControlPhase::Effective;result.detail="capture_active";return std::make_shared<const ControlSnapshot>(result);}
        return control;
    }
private:
    using Clock=std::chrono::steady_clock;
    struct Response { State state=State::Unavailable;int status=0;QJsonObject body; };
    struct Intent { std::uint64_t epoch=0;QString scene;bool capture=false;std::uint64_t capture_epoch=0;bool capture_revoked=false; };
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
    SceneClaims claims;std::set<QString> owners,denied,capture_owners;std::uint64_t control_epoch=0,intent_epoch=0;
    std::uint64_t capture_epoch=0,revoked_capture_epoch=0,target_manual_revision=0,capture_start_revision=0;
    QString superseded_scene;
    QString lease_id,lease_scene,lease_requested_scene;Descriptor lease_descriptor;SceneTarget target;
    bool pending=false,capture_applied=false,capture_running=false,capture_uncertain=false,lease_capture_revoked=false,lease_scene_revoked=false;unsigned lease_ttl=30;
    std::uint64_t blocked_epoch=~std::uint64_t(0);
    Clock::time_point renewed{},pending_since{};
    Intent DesiredUnlocked()const{return {intent_epoch,QString::fromStdString(claims.Scene()),!capture_owners.empty(),capture_epoch,
        revoked_capture_epoch!=0&&capture_epoch<=revoked_capture_epoch};}
    Intent Desired()const{std::lock_guard<std::mutex> lock(control_mutex);return DesiredUnlocked();}
    void IntentChanged(const Intent& previous)
    {
        const auto next=DesiredUnlocked();
        if(previous.scene==next.scene&&previous.capture==next.capture&&previous.capture_epoch==next.capture_epoch)return;
        if(previous.scene!=next.scene)superseded_scene.clear();
        ++intent_epoch;control_requested=true;wake.notify_all();
    }
    void Publish(Snapshot value){std::lock_guard<std::mutex> lock(snapshot_mutex);value.revision=snapshot->revision+1;snapshot=std::make_shared<const Snapshot>(std::move(value));}
    void PublishControl(ControlSnapshot value,std::uint64_t epoch)
    {std::lock_guard<std::mutex> lock(control_mutex);if(intent_epoch!=epoch)return;value.revision=control->revision+1;
        value.capture_active=capture_applied&&capture_running&&!lease_capture_revoked;value.minimum_capture_revision=capture_start_revision;
        control_epoch=epoch;control=std::make_shared<const ControlSnapshot>(std::move(value));}
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
        QObject::connect(&poll,&QTimer::timeout,&loop,[&]{
            if((stopping&&!cleanup)||(path.endsWith("/capture")&&!Desired().capture))
            {timed_out=true;reply->abort();loop.quit();}});
        QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timed_out=true;reply->abort();loop.quit();});
        poll.start();deadline.start(cleanup?750:(method=="GET"?config.request_timeout_ms:
            path.endsWith("/capture")?60000u:std::max(3000u,config.request_timeout_ms)));
        if(!reply->isFinished())loop.exec();consume();poll.stop();deadline.stop();
        Response result;result.status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto error=reply->error();delete reply;
        if(stopping&&!cleanup){result.state=State::Stopped;return result;}
        if(oversized){result.state=State::Incompatible;return result;}if(timed_out)return result;
        if(result.status==401||result.status==403){result.state=State::Unauthorized;return result;}
        // Qt reports HTTP4xx as network errors. The bounded JSON error code is
        // still parsed for lease policy; 3xx is never followed or accepted.
        if((result.status<200||result.status>=300)&&result.status!=409&&result.status!=404&&result.status!=503)return result;
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
    void ForgetLease()
    {
        lease_id.clear();lease_scene.clear();lease_requested_scene.clear();pending=false;capture_applied=false;capture_running=false;
        capture_uncertain=false;lease_capture_revoked=false;lease_scene_revoked=false;lease_ttl=30;target={};target_manual_revision=0;capture_start_revision=0;
    }
    void RevokeCapture(const Intent& intent)
    {
        lease_capture_revoked=true;
        std::lock_guard<std::mutex> lock(control_mutex);
        revoked_capture_epoch=std::max(revoked_capture_epoch,intent.capture_epoch);
    }
    bool SceneSupersededFor(const QString& scene)const
    {std::lock_guard<std::mutex> lock(control_mutex);return !superseded_scene.isEmpty()&&superseded_scene==scene;}
    void RevokeScene(const Intent& intent)
    {
        if(intent.scene.isEmpty())return;
        lease_scene_revoked=true;
        std::lock_guard<std::mutex> lock(control_mutex);superseded_scene=intent.scene;
    }
    static bool CaptureRunning(const QJsonObject& scene)
    {
        return scene.value("isRecording").isBool()&&scene.value("isRecording").toBool()&&
            scene.value("isPaused").isBool()&&!scene.value("isPaused").toBool()&&
            scene.value("isStopping").isBool()&&!scene.value("isStopping").toBool();
    }
    bool ObserveCaptureStatus(const QJsonObject& state,const Intent& intent)
    {
        const auto scene=state.value("scene").toObject();
        const auto revoked=scene.value("captureControlRevoked");
        if(revoked.toBool(false))
        {
            RevokeCapture(intent);ControlStatus(ControlPhase::Superseded,"capture_overridden",intent);return false;
        }
        capture_running=CaptureRunning(scene);
        // Appearance, canvas edits and source diagnostics advance the global
        // revision too. They never revoke an independent capture interest.
        std::uint64_t revision=0,scene_revision=0,manual=0;
        const auto actual_scene=scene.value("activeSceneId").toString().toLower();
        if(!Revision(state.value("revision"),revision)||!Revision(scene.value("stateRevision"),scene_revision,true)||
           (!scene.value("activeSceneId").isNull()&&!Guid(actual_scene)))
        {ControlStatus(ControlPhase::Unavailable,"invalid_capture_status",intent);return false;}
        Revision(scene.value("manualRevision"),manual,true);
        if(!lease_requested_scene.isEmpty()&&!intent.scene.isEmpty()&&(actual_scene!=lease_requested_scene||manual!=target_manual_revision))
            RevokeScene(intent);
        target={lease_descriptor.instance.toStdString(),revision,scene_revision,actual_scene.toStdString()};
        lease_scene=actual_scene;return true;
    }
    bool Release(QNetworkAccessManager& manager,bool cleanup=false)
    {
        if(lease_id.isEmpty())return true;
        auto r=Request(manager,lease_descriptor,"DELETE",Root+"leases/"+lease_id,{},cleanup);
        const QString error=r.body.value("error").toString();
        if((r.state==State::Ready&&(r.status==200||r.status==202||error=="lease_expired"||error=="lease_conflict"))||Clock::now()-renewed>std::chrono::seconds(lease_ttl+1))
        {ForgetLease();return true;}return false;
    }
    void HandleFailure(const Response& r,const Intent& intent,bool acquiring,bool capturing=false)
    {
        const QString code=r.body.value("error").toString();
        if(r.state==State::Ready&&(code=="command_in_progress"||code=="scene_busy"||code=="capture_busy")){ControlStatus(ControlPhase::Waiting,"command_busy",intent);return;}
        if(code=="state_superseded"||code=="manual_override")
        {
            if(capturing)RevokeCapture(intent);
            else if(!intent.scene.isEmpty())RevokeScene(intent);
            else blocked_epoch=intent.epoch;
            ControlStatus(ControlPhase::Superseded,"state_superseded",intent);return;
        }
        blocked_epoch=intent.epoch;
        if(code=="lease_conflict"){ControlStatus(ControlPhase::Conflict,"lease_conflict",intent);return;}
        if(code=="capture_failed"){ControlStatus(ControlPhase::Unavailable,"capture_failed",intent);return;}
        // A failed/aborted acquisition may have created ownership without
        // returning its capability. Never immediately reacquire in a loop.
        ControlStatus(ControlPhase::Unavailable,acquiring?"acquire_uncertain":"control_unavailable",intent);
    }
    bool AcceptTarget(const Response& r,const Descriptor& d,const Intent& intent,bool acquiring,bool renewal=false,bool capturing=false)
    {
        if(r.state!=State::Ready||(r.status!=200&&r.status!=202)){HandleFailure(r,intent,acquiring,capturing);return false;}
        const auto state=renewal?r.body:r.body.value("control").toObject();
        if(acquiring)
        {
            const auto id=state.value("lease").toObject().value("id").toString();
            if(!Guid(id)){blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"missing_lease_capability",intent);return false;}
            lease_id=id;lease_descriptor=d;lease_requested_scene=intent.scene;renewed=Clock::now();
        }
        const auto scene=state.value("scene").toObject();std::uint64_t revision=0,scene_revision=0;
        const auto actual_scene=scene.value("activeSceneId").toString().toLower();
        const bool valid=Revision(state.value("revision"),revision)&&Revision(scene.value("stateRevision"),scene_revision,true)&&
            (scene.value("activeSceneId").isNull()||Guid(actual_scene))&&
            (intent.scene.isEmpty()||actual_scene==intent.scene||((capturing||renewal)&&intent.capture));
        if(!valid){blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_control_ack",intent);return false;}
        if(!renewal)
        {
            std::uint64_t requested_revision=0,requested_scene=0;
            if(!Revision(r.body.value("targetControlRevision"),requested_revision)||!Revision(r.body.value("targetSceneRevision"),requested_scene,true)||requested_revision!=revision||requested_scene!=scene_revision)
            {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_control_target",intent);return false;}
        }
        if(capturing)
        {
            if(r.status!=200||r.body.value("result").toString()!="capture_applied"||!CaptureRunning(scene)||
               scene.value("captureControlRevoked").toBool(false)||
               !scene.value("isCaptureChanging").isBool()||scene.value("isCaptureChanging").toBool())
            {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_capture_ack",intent);return false;}
            capture_applied=true;capture_running=true;capture_start_revision=revision;
        }
        target={d.instance.toStdString(),revision,scene_revision,actual_scene.toStdString()};lease_scene=actual_scene;
        if(acquiring)Revision(scene.value("manualRevision"),target_manual_revision,true);
        if(acquiring||renewal)renewed=Clock::now();if(!renewal||!pending)pending_since=Clock::now();pending=true;
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
        {if(result==SceneResult::Superseded)RevokeScene(intent);blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Superseded,"state_superseded",intent);return;}
        if(result!=SceneResult::Effective){ControlStatus(ControlPhase::Pending,"pending_frame",intent);return;}
        // Bind the result to the discovered raw output as well as the exact
        // control/scene revision. The reader still validates TTL and dimensions.
        if(value.value("image").toObject().value("channel").toString()!=Read()->raw_channel)
        {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"invalid_effective_channel",intent);return;}
        pending=false;if(!intent.scene.isEmpty()&&target.scene_id!=intent.scene.toStdString()){ControlStatus(ControlPhase::Waiting,"scene_transition",intent);return;}ControlSnapshot v;v.phase=ControlPhase::Effective;v.detail="effective";v.scene_id=QString::fromStdString(target.scene_id);v.instance_id=d.instance;
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
        if(!lease_id.isEmpty()&&available&&lease_descriptor.instance!=d.instance){ForgetLease();blocked_epoch=~std::uint64_t(0);}
        // A timed-out start may still be preparing devices in Better. Its status,
        // not a lost HTTP response, decides when it is safe to restore the lease.
        if(capture_uncertain&&!lease_id.isEmpty())
        {
            QJsonObject status;const auto result=Get(manager,lease_descriptor,Root+"status",status);
            const auto scene=status.value("control").toObject().value("scene").toObject();
            if(result==State::Ready&&scene.value("isCaptureChanging").isBool()&&!scene.value("isCaptureChanging").toBool())capture_uncertain=false;
            else if(Clock::now()-renewed>std::chrono::seconds(lease_ttl+1))ForgetLease();
            else{ControlStatus(ControlPhase::Unavailable,"capture_outcome_unknown",intent);return;}
        }
        const bool wants_capture=intent.capture&&!intent.capture_revoked&&(!available||Read()->capabilities.contains("captureControl"));
        const bool wants_scene=!intent.scene.isEmpty()&&!SceneSupersededFor(intent.scene);
        if(!wants_scene&&!wants_capture)
        {
            if(!lease_id.isEmpty()){ControlStatus(ControlPhase::Releasing,"releasing",intent);if(!Release(manager))return;}
            ControlStatus(intent.capture_revoked||!intent.scene.isEmpty()?ControlPhase::Superseded:ControlPhase::Idle,
                intent.capture_revoked?"capture_overridden":!intent.scene.isEmpty()?"scene_overridden":intent.capture?"capture_control_unsupported":"idle",intent);return;
        }
        if(blocked_epoch==intent.epoch){if(!lease_id.isEmpty())Release(manager);return;}
        if(!available){ControlStatus(ControlPhase::Waiting,"discovery_unavailable",intent);return;}
        if(lease_id.isEmpty())
        {
            lease_ttl=wants_capture?300:30;
            QJsonObject body{{"clientId","openrgb-room/effects"},{"ttlSeconds",int(lease_ttl)}};
            if(wants_scene)body["sceneId"]=intent.scene;
            auto acquiring=intent;if(!wants_scene)acquiring.scene.clear();
            auto response=Request(manager,d,"POST",Root+"leases",body);
            AcceptTarget(response,d,acquiring,true);return;
        }
        // Removing one kind of interest must restore its baseline even if the
        // other kind remains. Reacquire only after DELETE completed; never stop
        // a pre-existing user recording with a blind capture=false command.
        if((capture_applied&&!wants_capture)||(!lease_requested_scene.isEmpty()&&intent.scene.isEmpty())||
           (lease_capture_revoked&&wants_capture)||(lease_scene_revoked&&wants_scene&&lease_requested_scene!=intent.scene))
        {ControlStatus(ControlPhase::Releasing,"restoring_previous_state",intent);Release(manager);return;}

        QJsonObject status;auto result=Get(manager,d,Root+"status",status);std::uint64_t current=0;
        const auto current_control=status.value("control").toObject();
        if(result!=State::Ready||!Revision(current_control.value("revision"),current))
        {ControlStatus(ControlPhase::Unavailable,"status_unavailable",intent);return;}
        if(wants_capture)
        {if(!ObserveCaptureStatus(current_control,intent))return;}
        else if(current>target.control_revision)
        {RevokeScene(intent);blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Superseded,"state_superseded",intent);return;}

        auto renew=[&](unsigned ttl)
        {
            Intent renewing=intent;renewing.scene=lease_scene;
            auto response=Request(manager,d,"PUT",Root+"leases/"+lease_id+"/renew",{{"ttlSeconds",int(ttl)}});
            if(AcceptTarget(response,d,renewing,false,true))lease_ttl=ttl;
        };
        if(!pending&&lease_requested_scene!=intent.scene&&wants_scene)
        {
            auto response=Request(manager,d,"PUT",Root+"leases/"+lease_id+"/scene",{{"sceneId",intent.scene}});
            if(AcceptTarget(response,d,intent,false))lease_requested_scene=intent.scene;
            return;
        }
        if(wants_capture&&!capture_applied)
        {
            if(lease_ttl!=300){renew(300);return;}
            auto response=Request(manager,d,"PUT",Root+"leases/"+lease_id+"/capture",{{"recording",true}});
            if(response.state!=State::Ready)capture_uncertain=true;
            AcceptTarget(response,d,intent,false,false,true);return;
        }
        if(capture_applied&&lease_ttl!=30){renew(30);return;}
        if(pending&&!capture_applied&&Clock::now()-pending_since>std::chrono::seconds(25))
        {blocked_epoch=intent.epoch;ControlStatus(ControlPhase::Unavailable,"pending_frame_timeout",intent);Release(manager);return;}
        if((!pending||capture_applied)&&Clock::now()-renewed>=std::chrono::seconds(10))
        {
            renew(30);return;
        }
        if(capture_applied&&!capture_running){ControlStatus(ControlPhase::Waiting,"capture_transition",intent);return;}
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
            const auto desired=Desired();
            auto deadline=next_discovery;if(!desired.scene.isEmpty()||desired.capture||!lease_id.isEmpty())deadline=std::min(deadline,Clock::now()+std::chrono::milliseconds(250));
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
RequestResult Discovery::RequestCapture(const QString& owner,bool enabled){return impl->RequestCapture(owner,enabled);}
void Discovery::ReleaseOwner(const QString& owner){impl->ReleaseOwner(owner);}
std::shared_ptr<const ControlSnapshot> Discovery::ReadControl(const QString& owner)const{return impl->ReadControl(owner);}
}
