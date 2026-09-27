/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BetterDiscovery.h"
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
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>

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
    explicit Impl(Config c):config(std::move(c)),snapshot(std::make_shared<const Snapshot>()){start();}
    ~Impl() override{stopping=true;wake.notify_all();wait();}
    std::shared_ptr<const Snapshot> Read()const{std::lock_guard<std::mutex> lock(snapshot_mutex);return snapshot;}
    void Refresh(){requested=true;wake.notify_all();}
private:
    const Config config;std::atomic<bool> stopping{false},requested{false};std::mutex sleep_mutex;
    std::condition_variable wake;mutable std::mutex snapshot_mutex;std::shared_ptr<const Snapshot> snapshot;
    void Publish(Snapshot value)
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex);value.revision=snapshot->revision+1;snapshot=std::make_shared<const Snapshot>(std::move(value));
    }
    State Get(QNetworkAccessManager& manager,const Descriptor& d,const QString& path,QJsonObject& json)
    {
        QUrl url=d.base;url.setPath(path);QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,QNetworkRequest::Manual);
        request.setRawHeader("Authorization",QByteArray("Bearer ")+d.token);request.setRawHeader("Accept","application/json");
        auto* reply=manager.get(request);reply->setReadBufferSize(ResponseLimit+1);
        QEventLoop loop;QTimer poll,deadline;poll.setInterval(20);deadline.setSingleShot(true);QByteArray bytes;bool oversized=false,timed_out=false;
        auto consume=[&]{bytes+=reply->read(ResponseLimit+1-bytes.size());if(bytes.size()>ResponseLimit){oversized=true;reply->abort();}};
        QObject::connect(reply,&QNetworkReply::readyRead,&loop,consume);
        QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
        QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(stopping){reply->abort();loop.quit();}});
        QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timed_out=true;reply->abort();loop.quit();});
        poll.start();deadline.start(config.request_timeout_ms);if(!reply->isFinished())loop.exec();consume();poll.stop();deadline.stop();
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto error=reply->error();delete reply;
        if(stopping)return State::Stopped;if(oversized)return State::Incompatible;if(timed_out)return State::Unavailable;
        if(status==401||status==403)return State::Unauthorized;
        // In particular, never follow 3xx even if Location is another loopback URL.
        if(status!=200||error!=QNetworkReply::NoError)return State::Unavailable;
        return Json(bytes,json)?State::Ready:State::Incompatible;
    }
    void run()override
    {
        QNetworkAccessManager manager;manager.setProxy(QNetworkProxy::NoProxy);
        while(!stopping)
        {
            requested=false;Descriptor d;Snapshot next;State state=Load(config.descriptor_path,d);
            if(state==State::Ready)
            {
                QJsonObject discovery,scenes;state=Get(manager,d,Root+"discovery",discovery);
                if(state==State::Ready&&!DiscoveryData(discovery,d,next))state=State::Incompatible;
                if(state==State::Ready)state=Get(manager,d,Root+"scenes",scenes);
                if(state==State::Ready&&!SceneData(scenes,next))state=State::Incompatible;
            }
            if(state!=State::Ready)next=Snapshot{};next.state=state;next.detail=QString::fromLatin1(StateName(state));Publish(std::move(next));
            std::unique_lock<std::mutex> lock(sleep_mutex);wake.wait_for(lock,std::chrono::milliseconds(config.refresh_ms),[&]{return stopping||requested;});
        }
        Snapshot end;end.state=State::Stopped;end.detail="stopped";Publish(std::move(end));
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
}
