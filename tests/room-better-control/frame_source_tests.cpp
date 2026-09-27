/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ScreenSources/BetterFrameSource.h"
#include <FrameSurface/FrameSurface.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace better_source;
static unsigned checks=0;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static bool Wait(const std::function<bool()>& predicate,int timeout=3000)
{
    QElapsedTimer timer;timer.start();
    do{QCoreApplication::processEvents();if(predicate())return true;QThread::msleep(2);}while(timer.elapsed()<timeout);
    return false;
}
static const QString Instance="12345678-1234-1234-1234-123456789abc";
static const QByteArray Token(64,'c'); // Synthetic, never a user's credential.
static QString Prefix(){return QString("BetterCapture-%1").arg(GetCurrentProcessId(),24,16,QLatin1Char('0'));}
static QJsonObject Reference(const QString& channel,const char* id,const room_surface::Frame& frame)
{
    return {{"outputId",id},{"channel",channel},{"generation",QString::number(frame.generation)},
            {"sequence",QString::number(frame.sequence)},{"width",int(frame.width)},{"height",int(frame.height)},
            {"stride",int(frame.stride)},{"format","BGRA8_OPAQUE_SRGB"}};
}
static room_surface::Frame ReadWire(const QString& channel)
{
    room_surface::Reader reader(channel.toStdString());room_surface::Frame frame;
    Check(reader.ReadLatest(frame,2000,100)==room_surface::FrameStatus::NewFrame,"test wire read");return frame;
}
static std::vector<std::uint8_t> Pixels(unsigned width,unsigned height,std::uint8_t blue,std::uint8_t red)
{
    std::vector<std::uint8_t> value(std::size_t(width)*height*4);
    for(std::size_t p=0;p<value.size();p+=4){value[p]=blue;value[p+1]=blue;value[p+2]=red;value[p+3]=255;}
    return value;
}
class Server
{
public:
    QTcpServer tcp; QString instance=Instance;std::map<QString,QJsonObject> states;
    unsigned states_requested=0,mutations=0,authenticated=0;bool hold_states=false;
    Server()
    {
        Check(tcp.listen(QHostAddress::LocalHost,0),"fixture listener");
        QObject::connect(&tcp,&QTcpServer::newConnection,&tcp,[this]{while(auto* socket=tcp.nextPendingConnection())
        {
            auto buffer=std::make_shared<QByteArray>();
            QObject::connect(socket,&QTcpSocket::readyRead,&tcp,[this,socket,buffer]
            {
                *buffer+=socket->readAll();if(!buffer->contains("\r\n\r\n"))return;
                QObject::disconnect(socket,&QTcpSocket::readyRead,&tcp,nullptr);
                const auto line=buffer->split('\n').front().split(' ');const QByteArray method=line.value(0),path=line.value(1);
                if(method!="GET")++mutations;
                if(buffer->contains(QByteArray("Bearer ")+Token))++authenticated;
                QJsonObject body;int status=200;
                if(path=="/api/native/v1/discovery")body=DiscoveryJson();
                else if(path=="/api/native/v1/scenes")body={{"apiVersion",1},{"scenes",QJsonArray{}}};
                else if(path.startsWith("/api/native/v1/states/"))
                {
                    ++states_requested;if(hold_states)return;
                    auto found=states.find(QString::fromLatin1(path.mid(22)));
                    if(found==states.end()){status=404;body={{"error","state_unavailable"}};}else body=found->second;
                }
                else{status=404;body={{"error","not_found"}};}
                auto json=QJsonDocument(body).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(json.size())+"\r\nConnection: close\r\n\r\n"+json);
                socket->disconnectFromHost();
            });
        }});
    }
    ~Server(){tcp.close();for(auto* socket:tcp.findChildren<QTcpSocket*>()){QObject::disconnect(socket,nullptr,&tcp,nullptr);socket->abort();}}
    QJsonObject DiscoveryJson() const
    {
        return {{"apiVersion",1},{"application","BetterSignalRGBScreenCapture"},{"instanceId",instance},
          {"transport",QJsonObject{{"name","ORGBFRM1"},{"version",1},{"headerBytes",128},{"maxCapacity",67108864},{"publisherCapacity",1920000},{"scope","windows-current-user-session"},{"recommendedTtlMs",600}}},
          {"canvas",QJsonObject{{"width",320},{"height",200}}},{"capabilities",QJsonArray{"raw-composite","opacity-coverage","saved-scenes","rendering-metadata-v1"}},
          {"outputs",QJsonArray{QJsonObject{{"id","canvas-raw"},{"channel",Prefix()+"-Raw"},{"role","opaque-black-composite"},{"format","BGRA8_OPAQUE_SRGB"}},
                               QJsonObject{{"id","canvas-coverage"},{"channel",Prefix()+"-Coverage"},{"role","opacity-coverage-grayscale"},{"format","BGRA8_OPAQUE_SRGB"}}}}};
    }
    void Descriptor(const QString& path)
    {
        QFile file(path);Check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"fixture descriptor");
        file.write(QJsonDocument(QJsonObject{{"apiVersion",1},{"baseUrl",QString("http://127.0.0.1:%1").arg(tcp.serverPort())},
            {"authorizationScheme","Bearer"},{"token",QString::fromLatin1(Token)},{"instanceId",instance},{"processId",int(GetCurrentProcessId())},
            {"discoveryPath","/api/native/v1/discovery"}}).toJson(QJsonDocument::Compact));
    }
    QJsonObject Bind(const room_surface::Frame& raw,const room_surface::Frame& coverage,unsigned revision=1)
    {
        return {{"stateRevision",int(revision)},{"controlRevision",int(revision)},
          {"scene",QJsonObject{{"stateRevision",int(revision)},{"activeSceneId",QJsonValue::Null},{"sceneLoading",false}}},
          {"image",Reference(Prefix()+"-Raw","canvas-raw",raw)},{"coverage",Reference(Prefix()+"-Coverage","canvas-coverage",coverage)},
          {"rendering",QJsonObject{{"version",1},{"schema","better.native-rendering"},{"stateRevision",int(revision)},
            {"canvasWidth",320},{"canvasHeight",200},{"outputWidth",int(raw.width)},{"outputHeight",int(raw.height)},
            {"effectiveSettings",QJsonObject{{"pictureMode","Standard"}}},{"sources",QJsonArray{}}}}};
    }
};
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    try
    {
        QTemporaryDir directory;Check(directory.isValid(),"temporary fixture directory");Server server;
        const auto path=directory.filePath("connection.json");server.Descriptor(path);
        Config config;config.descriptor_path=path;config.refresh_ms=100;config.request_timeout_ms=250;
        auto discovery=Discovery::Acquire(config);auto source=FrameSource::Acquire(discovery);
        Check(FrameSource::Acquire(discovery)==source,"one shared frame provider");
        Check(Wait([&]{return discovery->Read()->Ready();}),"discovery ready");
        Check(!source->Read()->Usable(),"no producer no frame");
        auto raw=std::make_unique<room_surface::Publisher>((Prefix()+"-Raw").toStdString(),1920000);
        auto coverage=std::make_unique<room_surface::Publisher>((Prefix()+"-Coverage").toStdString(),1920000);
        Check(raw->IsOpen()&&coverage->IsOpen(),"synthetic publishers open");
        auto pixels=Pixels(800,600,17,63),mask=Pixels(800,600,128,128);
        Check(coverage->PublishBGRA(mask.data(),mask.size(),800,600,3200),"coverage first");
        Check(raw->PublishBGRA(pixels.data(),pixels.size(),800,600,3200),"raw publish");
        auto r=ReadWire(Prefix()+"-Raw"),c=ReadWire(Prefix()+"-Coverage");
        Check(Wait([&]{return server.states_requested>0;}),"raw triggers async state GET");
        Check(!source->Read()->Usable(),"404 pending state cannot render");
        server.states[QString::number(r.generation)]=server.Bind(r,c);
        Check(Wait([&]{return source->Read()->Usable();}),"retry pairs immutable metadata");
        auto original=source->Read();
        Check(original->raw.size()==QSize(800,600)&&original->coverage.size()==QSize(800,600),"full resolution preserved");
        Check(qBlue(original->raw.pixel(2,2))==17&&qRed(original->raw.pixel(2,2))==63,"raw BGRA channel order");
        Check(qRed(original->coverage.pixel(2,2))==128&&original->metadata.value("schema")=="better.native-rendering","coverage and rendering metadata bound");
        Check(original->state_revision==1&&original->raw_generation==r.generation&&original->coverage_generation==c.generation,"exact stamps exposed");
        QImage edited=original->raw;edited.setPixelColor(2,2,Qt::white);
        Check(qBlue(original->raw.pixel(2,2))==17,"snapshots remain immutable");
        const auto requests=server.states_requested;pixels=Pixels(800,600,22,33);
        Check(raw->PublishBGRA(pixels.data(),pixels.size(),800,600,3200),"same generation next frame");
        Check(Wait([&]{return source->Read()->Usable()&&source->Read()->raw_sequence==2;}),"new raw sequence shares first-frame state");
        Check(server.states_requested==requests,"metadata cached per generation");
        Check(qBlue(original->raw.pixel(2,2))==17,"old image preserved after publish");

        coverage.reset();coverage=std::make_unique<room_surface::Publisher>((Prefix()+"-Coverage").toStdString(),1920000);
        Check(coverage->IsOpen()&&coverage->PublishBGRA(mask.data(),mask.size(),800,600,3200),"replacement coverage");
        Check(Wait([&]{return source->Read()->state==FrameState::WaitingCoverage;}),"coverage new generation refuses old raw");
        raw.reset();raw=std::make_unique<room_surface::Publisher>((Prefix()+"-Raw").toStdString(),1920000);
        Check(raw->IsOpen()&&raw->PublishBGRA(pixels.data(),pixels.size(),800,600,3200),"replacement raw");
        r=ReadWire(Prefix()+"-Raw");c=ReadWire(Prefix()+"-Coverage");server.states[QString::number(r.generation)]=server.Bind(r,c,2);
        Check(Wait([&]{return source->Read()->Usable()&&source->Read()->state_revision==2;}),"new pair recovers without old state");
        Check(Wait([&]{return !source->Read()->Usable();},1500),"no heartbeat expires paired frame");
        Check(!original->Usable(),"retained old snapshot also expires");

        // Malformed mask from a fresh generation fails closed even if metadata pairs.
        raw.reset();coverage.reset();raw=std::make_unique<room_surface::Publisher>((Prefix()+"-Raw").toStdString(),1920000);
        coverage=std::make_unique<room_surface::Publisher>((Prefix()+"-Coverage").toStdString(),1920000);
        auto colored_mask=Pixels(800,600,128,127);
        Check(coverage->PublishBGRA(colored_mask.data(),colored_mask.size(),800,600,3200)&&raw->PublishBGRA(pixels.data(),pixels.size(),800,600,3200),"invalid mask fixture");
        r=ReadWire(Prefix()+"-Raw");c=ReadWire(Prefix()+"-Coverage");server.states[QString::number(r.generation)]=server.Bind(r,c,3);
        Check(Wait([&]{return source->Read()->state==FrameState::Invalid;}),"nongrayscale coverage rejected");
        Check(!source->Read()->Usable()&&source->Read()->raw.isNull(),"invalid pair exposes no image");
        raw.reset();coverage.reset();raw=std::make_unique<room_surface::Publisher>((Prefix()+"-Raw").toStdString(),1920000);
        coverage=std::make_unique<room_surface::Publisher>((Prefix()+"-Coverage").toStdString(),1920000);
        Check(coverage->PublishBGRA(mask.data(),mask.size(),800,600,3200)&&raw->PublishBGRA(pixels.data(),pixels.size(),800,600,3200),"invalid metadata fixture");
        r=ReadWire(Prefix()+"-Raw");c=ReadWire(Prefix()+"-Coverage");
        auto invalid=server.Bind(r,c,4);auto rendering=invalid["rendering"].toObject();rendering["stateRevision"]=3;invalid["rendering"]=rendering;
        server.states[QString::number(r.generation)]=invalid;
        Check(Wait([&]{return source->Read()->state==FrameState::Invalid;}),"mismatched rendering revision rejected through actual HTTP");

        server.instance="bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";server.Descriptor(path);server.states.clear();discovery->Refresh();
        Check(Wait([&]{return discovery->Read()->Ready()&&discovery->Read()->instance_id==server.instance;}),"new producer instance discovered");
        Check(Wait([&]{return source->Read()->state==FrameState::WaitingState;}),"old instance metadata cache purged");
        Check(!source->Read()->Usable(),"new instance cannot expose old ready pair");
        Check(server.mutations==0&&server.authenticated>0,"passive provider never acquires leases or changes scene");
        server.hold_states=true;const auto before=server.states_requested;
        Check(Wait([&]{return server.states_requested>before;}),"pending metadata request observed");
        QElapsedTimer stop;stop.start();source.reset();Check(stop.elapsed()<250,"frame worker teardown bounded");
        stop.restart();discovery.reset();Check(stop.elapsed()<500,"pending HTTP teardown bounded separately");
        std::cout<<"PASS "<<checks<<" Better frame source checks (synthetic HTTP + actual ORGBFRM1)\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
