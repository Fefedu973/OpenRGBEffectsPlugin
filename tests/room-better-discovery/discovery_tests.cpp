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
#include <atomic>
#include <thread>
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
 const int header_end=buffer->indexOf("\r\n\r\n");int content_length=0;
 for(auto line:buffer->left(header_end).split('\n'))if(line.toLower().startsWith("content-length:"))content_length=line.mid(15).trimmed().toInt();
 if(buffer->size()<header_end+4+content_length)return;
 ++requests;bool auth=false;for(auto line:buffer->split('\n')){line=line.trimmed();auto colon=line.indexOf(':');if(colon<0)continue;auto key=line.left(colon).toLower(),value=line.mid(colon+1).trimmed();if(key=="authorization"&&value==QByteArray("Bearer ")+Token)auth=true;if(key=="cookie")++cookies;if(key=="origin"||key=="sec-fetch-site")++unexpected;}
 if(auth)++authenticated;const auto path=buffer->split(' ').value(1);if(path!="/api/native/v1/discovery"&&path!="/api/native/v1/scenes")++unexpected;
 QObject::disconnect(socket,&QTcpSocket::readyRead,&tcp,nullptr);if(hang)return;
 auto requestBody=QJsonDocument::fromJson(buffer->mid(header_end+4,content_length)).object();
 auto value=route?route(buffer->split(' ').value(0),path,requestBody):(path.endsWith("scenes")?scenes:discovery);
 QByteArray body=raw.isEmpty()?QJsonDocument(value).toJson(QJsonDocument::Compact):raw;
 QByteArray response="HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\nSet-Cookie: unexpected=yes\r\n";
 if(!location.isEmpty())response+="Location: "+location+"\r\n";response+="\r\n";socket->write(response);socket->write(body);socket->disconnectFromHost();
 });}});}
 ~Server(){tcp.close();for(auto* s:tcp.findChildren<QTcpSocket*>()){QObject::disconnect(s,nullptr,&tcp,nullptr);s->abort();}}
 std::function<QJsonObject(QByteArray,QByteArray,QJsonObject)> route;
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
 // Optional scene control is exercised only after all read-only guards.
 const QString second="11111111-2222-3333-4444-555555555555",lease="eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
 QString activeScene;unsigned revision=1,sceneRevision=1,acquires=0,renews=0,deletes=0,changes=0,stateGets=0;bool effective=false,releaseDuringAcquire=false;QString acquireError;int stateError=0;
 auto Control=[&]{return QJsonObject{{"revision",int(revision)},{"lease",QJsonObject{{"id",lease}}},{"scene",QJsonObject{{"activeSceneId",activeScene},{"stateRevision",int(sceneRevision)},{"sceneLoading",false}}}};};
 auto Envelope=[&](QString generation="18446744073709551000"){return QJsonObject{{"controlRevision",int(revision)},{"scene",QJsonObject{{"stateRevision",int(sceneRevision)},{"activeSceneId",activeScene},{"sceneLoading",false}}},{"image",QJsonObject{{"outputId","canvas-raw"},{"channel",Prefix+"-Raw"},{"format","BGRA8_OPAQUE_SRGB"},{"generation",generation},{"sequence","7"}}}};};
 auto Ack=[&]{return QJsonObject{{"apiVersion",1},{"result",effective?"effective":"pending_frame"},{"control",Control()},{"targetControlRevision",int(revision)},{"targetSceneRevision",int(sceneRevision)},{"effectiveState",effective?QJsonValue(Envelope()):QJsonValue(QJsonValue::Null)}};};
 server.route=[&](QByteArray method,QByteArray endpoint,QJsonObject body)->QJsonObject
 {
   server.status=200;
   if(endpoint.endsWith("discovery"))return server.discovery;if(endpoint.endsWith("scenes"))return server.scenes;
   if(endpoint=="/api/native/v1/leases"&&method=="POST")
   {++acquires;if(!acquireError.isEmpty()){server.status=409;return {{"apiVersion",1},{"error",acquireError}};}Check(body.value("ttlSeconds").toInt()==30&&!body.contains("overrides"),"lease has TTL30 and no appearance mutation");activeScene=body.value("sceneId").toString();++revision;if(releaseDuringAcquire)client->ReleaseScene("race-owner");server.status=effective?200:202;return Ack();}
   if(endpoint.endsWith("/renew")){++renews;++revision;return Control();}
   if(endpoint.endsWith("/scene")){++changes;++sceneRevision;++revision;activeScene=body.value("sceneId").toString();server.status=effective?200:202;return Ack();}
   if(method=="DELETE"){++deletes;++revision;activeScene.clear();server.status=effective?200:202;return Ack();}
   if(endpoint.endsWith("/status"))return {{"apiVersion",1},{"control",Control()},{"output",QJsonObject{{"effectiveState",effective?QJsonValue(Envelope()):QJsonValue(QJsonValue::Null)}}}};
   if(endpoint.startsWith("/api/native/v1/states/")){++stateGets;if(stateError==404){server.status=404;return {{"apiVersion",1},{"error","state_unavailable"}};}return Envelope(stateError==1?QString("999999"):QString::fromLatin1(endpoint.mid(22)));}
   server.status=404;return {{"error","not_found"}};
 };
 Check(acquires==0,"raw discovery did not acquire control");Check(client->RequestScene("effect-one",SceneId)==RequestResult::Accepted,"explicit scene claim accepted");
 Check(Wait([&]{return client->ReadControl("effect-one")->phase==ControlPhase::Pending;}),"202 creates pending state");Check(client->ReadControl("effect-one")->effective_generation==0,"pending frame cannot expose generation");
 Check(client->RequestScene("effect-two",SceneId)==RequestResult::Accepted,"same scene shared between effects");Check(client->RequestScene("conflicting",second)==RequestResult::Conflict,"different global scene refused");Check(client->ReadControl("conflicting")->phase==ControlPhase::Conflict,"owner-specific conflict visible");
 Check(client->RequestScene("bad-owner","not-a-guid")==RequestResult::Invalid,"invalid scene UUID refused");
 QElapsedTimer pendingTime;pendingTime.start();Check(Wait([&]{return pendingTime.elapsed()>10300;},11000),"pending observation interval");Check(renews==0&&acquires==1,"pending acquisition is neither renewed nor reacquired");
 effective=true;client->Refresh();Check(Wait([&]{return client->ReadControl("effect-one")->phase==ControlPhase::Effective;}),"status exact scene/revision acknowledges frame");
 auto bound=client->ReadControl("effect-one");Check(bound->effective_generation==18446744073709551000ULL&&bound->minimum_sequence==7,"uint64 generation remains exact beyond double precision");
 Check(Wait([&]{return renews>0&&client->ReadControl("effect-one")->phase==ControlPhase::Effective;}),"renew occurs only after effective ack and rechecks its new revision");
 client->ReleaseScene("effect-one");Check(client->ReadControl("effect-one")->phase==ControlPhase::Idle,"released owner becomes idle independently");Check(deletes==0,"shared lease survives first effect stop");
 Check(client->RequestScene("effect-two",second)==RequestResult::Accepted,"sole owner can select another scene");Check(Wait([&]{return changes==1&&client->ReadControl("effect-two")->phase==ControlPhase::Effective;}),"scene change uses existing lease and exact new target");
 client->ReleaseScene("effect-two");Check(Wait([&]{return deletes==1;}),"last owner releases capability");client->ReleaseScene("conflicting");
 // GET states shares the same authenticated transport without ownership.
 client->RequestState(123);Check(Wait([&]{auto v=client->ReadState(123);return v&&v->status!=PublishedStateStatus::Pending;}),"state request completed");auto published=client->ReadState(123);
 Check(published->status==PublishedStateStatus::Ready&&published->instance_id==Instance&&published->envelope.value("image").toObject().value("generation").toString()=="123","state generation bound to instance");
 int beforeStates=stateGets;client->RequestState(123);Check(Wait([]{return true;}),"read cache");Check(stateGets==beforeStates,"immutable state cache avoids repeat HTTP");
 for(unsigned gen=124;gen<=133;++gen){client->RequestState(gen);Check(Wait([&]{auto v=client->ReadState(gen);return v&&v->status==PublishedStateStatus::Ready;}),"new state cached");}
 Check(!client->ReadState(123)&&published->status==PublishedStateStatus::Ready,"cache bounded to8 while held immutable snapshot survives");
 stateError=404;client->RequestState(999);Check(Wait([&]{auto v=client->ReadState(999);return v&&v->status==PublishedStateStatus::Unavailable;}),"404 is retryable unavailable state");beforeStates=stateGets;client->RequestState(999);QElapsedTimer throttle;throttle.start();Wait([&]{return throttle.elapsed()>100;},200);Check(stateGets==beforeStates,"404 retry throttled");stateError=0;Wait([&]{return throttle.elapsed()>300;},400);client->RequestState(999);Check(Wait([&]{return client->ReadState(999)->status==PublishedStateStatus::Ready;}),"state retry recovers after commit");
 stateError=1;client->RequestState(1000);Check(Wait([&]{auto v=client->ReadState(1000);return v&&v->status==PublishedStateStatus::Invalid;}),"wrong returned generation never becomes Ready");stateError=0;
  acquireError="lease_conflict";Check(client->RequestScene("external-conflict",SceneId)==RequestResult::Accepted,"local claim awaits remote control result");Check(Wait([&]{return client->ReadControl("external-conflict")->phase==ControlPhase::Conflict;}),"remote lease conflict visible");auto beforeAcquire=acquires;client->RequestScene("external-conflict",SceneId);QElapsedTimer conflictTime;conflictTime.start();Wait([&]{return conflictTime.elapsed()>650;},800);Check(acquires==beforeAcquire,"remote conflict is not blindly reacquired");client->ReleaseScene("external-conflict");acquireError.clear();
 client->RequestScene("manual-precedence",SceneId);Check(Wait([&]{return client->ReadControl("manual-precedence")->phase==ControlPhase::Effective;}),"new explicit claim after conflict succeeds");++revision;
 Check(Wait([&]{return client->ReadControl("manual-precedence")->phase==ControlPhase::Superseded;}),"higher manual revision does not masquerade as effect completion");Check(Wait([&]{return deletes==2;}),"superseded owned lease released");beforeAcquire=acquires;conflictTime.restart();Wait([&]{return conflictTime.elapsed()>650;},800);Check(acquires==beforeAcquire,"manual edit wins without automatic reacquisition");client->ReleaseScene("manual-precedence");
 const auto beforeRaceDeletes=deletes;releaseDuringAcquire=true;client->RequestScene("race-owner",SceneId);Check(Wait([&]{return deletes>beforeRaceDeletes;}),"late acquisition capability released after owner stopped during HTTP");Check(client->ReadControl("race-owner")->phase==ControlPhase::Idle,"cancelled owner never sees stale effective ack");releaseDuringAcquire=false;
 const auto beforeStopDeletes=deletes;client->RequestScene("cleanup-owner",SceneId);Check(Wait([&]{return client->ReadControl("cleanup-owner")->phase==ControlPhase::Effective;}),"owned lease ready before provider destruction");
 std::atomic<bool> destroyed{false};std::thread destroyer([held=std::move(client),&destroyed]()mutable{held.reset();destroyed=true;});Check(Wait([&]{return destroyed.load();},1500),"provider stop completes bounded release without caller event dependency");destroyer.join();Check(deletes>beforeStopDeletes,"provider destruction sends owned DELETE capability");
 server.route={};client=Discovery::Acquire(c);Check(Wait([&]{return client->Read()->Ready();}),"fresh provider after cleanup starts normally");

 QFile::remove(path);Check(Refresh(client)->state==State::MissingDescriptor,"disable removes ready channels");Write(path,Descriptor(server.Port()));server.hang=true;auto count=server.requests;client->Refresh();Check(Wait([&]{return server.requests>count;}),"pending request observed");QElapsedTimer stop;stop.start();client.reset();Check(stop.elapsed()<500,"shutdown aborts pending network request without GUI callback or timeout wait");
 bool invalid=false;try{Config bad;bad.descriptor_path="relative.json";Discovery::Acquire(bad);}catch(const std::invalid_argument&){invalid=true;}Check(invalid,"relative descriptor paths rejected");
 QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);std::cout<<"PASS "<<checks<<" Better discovery checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
