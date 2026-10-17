#pragma once
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
#include <QTimer>
#include <QPointer>
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
 QTcpServer tcp;QJsonObject discovery=DiscoveryJSON(),scenes=ScenesJSON();int status=200;QByteArray raw,location;bool hang=false;
 unsigned delay_ms=0;bool drop_response=false;int requests=0,authenticated=0,unexpected=0,cookies=0;
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
 if(!location.isEmpty())response+="Location: "+location+"\r\n";response+="\r\n";
 if(drop_response){socket->abort();return;}
 const QPointer<QTcpSocket> guarded(socket);
 const auto send=[guarded,response,body]{if(guarded&&guarded->state()==QAbstractSocket::ConnectedState){guarded->write(response);guarded->write(body);guarded->disconnectFromHost();}};
 if(delay_ms)QTimer::singleShot(delay_ms,socket,send);else send();
 });}});}
 ~Server(){tcp.close();for(auto* s:tcp.findChildren<QTcpSocket*>()){QObject::disconnect(s,nullptr,&tcp,nullptr);s->abort();}}
 std::function<QJsonObject(QByteArray,QByteArray,QJsonObject)> route;
 unsigned Port()const{return tcp.serverPort();}
};
static void Write(const QString& file,const QJsonObject& j){QFile f(file);Check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"descriptor fixture write");f.write(QJsonDocument(j).toJson(QJsonDocument::Compact));}
static std::shared_ptr<const Snapshot> Refresh(const std::shared_ptr<Discovery>& d)
{auto rev=d->Read()->revision;d->Refresh();Check(Wait([&]{return d->Read()->revision>rev;}),"refresh bounded");return d->Read();}
