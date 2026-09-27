/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BetterDiscovery.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace better_source;
static unsigned checks=0;
static void Check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
static const QString Instance="12345678-1234-1234-1234-123456789abc";
static const QString SceneId="aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
static const QByteArray Token(64,'a');
static const QString Prefix="BetterCapture-0123456789abcdef01234567";
static bool Wait(const std::function<bool()>& predicate,int ms=2500)
{QElapsedTimer t;t.start();while(!predicate()&&t.elapsed()<ms){QCoreApplication::processEvents();QThread::msleep(2);}return predicate();}
static QJsonObject Descriptor(unsigned port)
{return {{"apiVersion",1},{"baseUrl",QString("http://127.0.0.1:%1").arg(port)},{"authorizationScheme","Bearer"},{"token",QString::fromLatin1(Token)},{"instanceId",Instance},{"processId",1234},{"discoveryPath","/api/native/v1/discovery"}};}
static QJsonObject DiscoveryJSON()
{
 return {{"apiVersion",1},{"application","BetterSignalRGBScreenCapture"},{"instanceId",Instance},
 {"transport",QJsonObject{{"name","ORGBFRM1"},{"version",1},{"headerBytes",128},{"maxCapacity",67108864},{"publisherCapacity",1920000},{"scope","windows-current-user-session"},{"recommendedTtlMs",2000}}},
 {"canvas",QJsonObject{{"width",320},{"height",200}}},{"capabilities",QJsonArray{"raw-composite","opacity-coverage","saved-scenes","static-heartbeat","rendering-metadata-v1"}},
 {"outputs",QJsonArray{QJsonObject{{"id","canvas-raw"},{"channel",Prefix+"-Raw"},{"format","BGRA8_OPAQUE_SRGB"},{"role","opaque-black-composite"}},QJsonObject{{"id","canvas-coverage"},{"channel",Prefix+"-Coverage"},{"format","BGRA8_OPAQUE_SRGB"},{"role","opacity-coverage-grayscale"}}}}};
}
static QJsonObject ScenesJSON(){return {{"apiVersion",1},{"scenes",QJsonArray{QJsonObject{{"id",SceneId},{"name","Desk scene"}}}}};}
class Server
{
public:
 QTcpServer tcp;QJsonObject discovery=DiscoveryJSON(),scenes=ScenesJSON();int status=200;QByteArray raw,location;bool hang=false;int requests=0,authenticated=0,unexpected=0,cookies=0;
 Server(){Check(tcp.listen(QHostAddress::LocalHost,0),"synthetic listener");QObject::connect(&tcp,&QTcpServer::newConnection,&tcp,[this]{while(auto* socket=tcp.nextPendingConnection()){
 auto buffer=std::make_shared<QByteArray>();QObject::connect(socket,&QTcpSocket::readyRead,&tcp,[this,socket,buffer]{*buffer+=socket->readAll();if(!buffer->contains("\r\n\r\n"))return;
 ++requests;bool auth=false;for(auto line:buffer->split('\n')){line=line.trimmed();auto colon=line.indexOf(':');if(colon<0)continue;auto key=line.left(colon).toLower(),value=line.mid(colon+1).trimmed();if(key=="authorization"&&value==QByteArray("Bearer ")+Token)auth=true;if(key=="cookie")++cookies;if(key=="origin"||key=="sec-fetch-site")++unexpected;}
 if(auth)++authenticated;const auto path=buffer->split(' ').value(1);if(path!="/api/native/v1/discovery"&&path!="/api/native/v1/scenes")++unexpected;
 QObject::disconnect(socket,&QTcpSocket::readyRead,&tcp,nullptr);if(hang)return;
 QByteArray body=raw.isEmpty()?QJsonDocument(path.endsWith("scenes")?scenes:discovery).toJson(QJsonDocument::Compact):raw;
 QByteArray response="HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\nSet-Cookie: unexpected=yes\r\n";
 if(!location.isEmpty())response+="Location: "+location+"\r\n";response+="\r\n";socket->write(response);socket->write(body);socket->disconnectFromHost();
 });}});}
 ~Server(){tcp.close();for(auto* s:tcp.findChildren<QTcpSocket*>()){QObject::disconnect(s,nullptr,&tcp,nullptr);s->abort();}}
 unsigned Port()const{return tcp.serverPort();}
};
static void Write(const QString& file,const QJsonObject& j){QFile f(file);Check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"descriptor fixture write");f.write(QJsonDocument(j).toJson(QJsonDocument::Compact));}
static std::shared_ptr<const Snapshot> Refresh(const std::shared_ptr<Discovery>& d)
{auto rev=d->Read()->revision;d->Refresh();Check(Wait([&]{return d->Read()->revision>rev;}),"refresh bounded");return d->Read();}
int main(int argc,char** argv)
{
 QCoreApplication app(argc,argv);
 try{
 QTemporaryDir dir;Check(dir.isValid(),"private temporary fixture");const QString path=dir.filePath("connection.json");Server server,proxy;
 QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",quint16(proxy.Port())));
 Config c;c.descriptor_path=path;c.refresh_ms=60000;c.request_timeout_ms=250;
 auto client=Discovery::Acquire(c);Check(Wait([&]{return client->Read()->revision>0;}),"worker starts");Check(client->Read()->state==State::MissingDescriptor,"missing descriptor is normal and never guesses channel");
 Write(path,Descriptor(server.Port()));auto ready=Refresh(client);Check(ready->Ready(),"authenticated discovery succeeds");Check(ready->raw_channel==Prefix+"-Raw"&&ready->coverage_channel==Prefix+"-Coverage","real channels discovered");
 Check(ready->scenes.size()==1&&ready->scenes[0].id==SceneId&&ready->scenes[0].name=="Desk scene","exact saved scene DTO parsed");Check(ready->canvas_width==320&&ready->canvas_height==200&&ready->recommended_ttl_ms==2000,"canvas geometry and TTL");
 Check(server.requests==2&&server.authenticated==2&&server.unexpected==0&&server.cookies==0,"both requests authenticated, no browser headers or cookies");Check(proxy.requests==0,"configured system proxy bypassed");
 auto shared=Discovery::Acquire(c);Check(shared==client,"one worker for identical configuration");shared.reset();
 for(const QString bad:{"https://127.0.0.1:1234","http://localhost:1234","http://127.0.0.2:1234","http://example.com:1234","http://user:secret@127.0.0.1:1234","http://127.0.0.1:1234/path","http://127.0.0.1:1234/?token=x","http://127.0.0.1:0","http://127.0.0.1:65536","http://127.0.0.1:1234#fragment","http://127.0.0.1:1234/"})
 {auto j=Descriptor(server.Port());j["baseUrl"]=bad;Write(path,j);int before=server.requests;auto snap=Refresh(client);Check(snap->state==State::InvalidDescriptor&&snap->raw_channel.isEmpty(),"unsafe base rejected before I/O");Check(server.requests==before,"invalid base sent no credential");}
 for(const QString field:{"token","authorizationScheme","discoveryPath","instanceId","apiVersion","processId"})
 {auto j=Descriptor(server.Port());j[field]="malformed";Write(path,j);int before=server.requests;Check(Refresh(client)->state==State::InvalidDescriptor,"malformed descriptor field rejected");Check(server.requests==before,"malformed auth sent no request");}
 auto fractional=Descriptor(server.Port());fractional["apiVersion"]=1.5;Write(path,fractional);Check(Refresh(client)->state==State::InvalidDescriptor,"fractional API version rejected");
 Write(path,Descriptor(server.Port()));server.status=302;server.location=QString("http://127.0.0.1:%1/leak").arg(proxy.Port()).toLatin1();Check(Refresh(client)->state==State::Unavailable&&proxy.requests==0,"redirect never forwards Bearer");
 server.status=401;server.location.clear();Check(Refresh(client)->state==State::Unauthorized,"401 discards formerly ready channels");Check(ready->Ready()&&ready->scenes.size()==1,"published snapshots are immutable");
 server.status=200;server.discovery["instanceId"]="bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";Check(Refresh(client)->state==State::Incompatible,"descriptor/response instance mismatch rejected");server.discovery=DiscoveryJSON();
 auto transport=server.discovery["transport"].toObject();transport["headerBytes"]=129;server.discovery["transport"]=transport;Check(Refresh(client)->state==State::Incompatible,"wrong binary transport version rejected");server.discovery=DiscoveryJSON();
 auto outputs=server.discovery["outputs"].toArray();auto channel=outputs[0].toObject();channel["channel"]="Local\\arbitrary-mapping";outputs[0]=channel;server.discovery["outputs"]=outputs;Check(Refresh(client)->state==State::Incompatible,"invalid output channel rejected");server.discovery=DiscoveryJSON();
 server.scenes["scenes"]=QJsonArray{QJsonObject{{"id",SceneId},{"name","one"}},QJsonObject{{"id",SceneId},{"name","two"}}};Check(Refresh(client)->state==State::Incompatible,"duplicate scene identity rejected");server.scenes=ScenesJSON();
 server.raw=QByteArray(262145,'x');Check(Refresh(client)->state==State::Incompatible,"response size bounded");server.raw="{broken";Check(Refresh(client)->state==State::Incompatible,"invalid JSON rejected");server.raw.clear();
 server.hang=true;Check(Refresh(client)->state==State::Unavailable,"silent peer timeout bounded");server.hang=false;Check(Refresh(client)->Ready(),"recovery uses fresh descriptor and discovery");
 QFile::remove(path);Check(Refresh(client)->state==State::MissingDescriptor,"disable removes ready channels");Write(path,Descriptor(server.Port()));server.hang=true;auto count=server.requests;client->Refresh();Check(Wait([&]{return server.requests>count;}),"pending request observed");QElapsedTimer stop;stop.start();client.reset();Check(stop.elapsed()<500,"shutdown aborts pending network request without GUI callback or timeout wait");
 bool invalid=false;try{Config bad;bad.descriptor_path="relative.json";Discovery::Acquire(bad);}catch(const std::invalid_argument&){invalid=true;}Check(invalid,"relative descriptor paths rejected");
 QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);std::cout<<"PASS "<<checks<<" Better discovery checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
