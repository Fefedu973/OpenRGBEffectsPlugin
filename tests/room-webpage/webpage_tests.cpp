/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WebPageCapture.h"
#include "CanvasRouting.h"
#include <QApplication>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QBuffer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <iostream>
#include <cstdlib>

struct ImageSink : room_image::RGBControllerImageInterface
{
    std::shared_ptr<const room_image::Frame> received;
    room_image::Mapping mapping;
    bool GetImageOutput(unsigned zone,room_image::Output& out) const override {out={zone,800,600,30};return zone==0;}
    room_image::SubmitResult SubmitImage(unsigned,std::shared_ptr<const room_image::Frame> frame,const room_image::Mapping& m,unsigned) override
    {received=std::move(frame);mapping=m;return room_image::SubmitResult::Accepted;}
};

int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    unsigned checks=0;
    auto check=[&](bool value){++checks;if(!value){std::cerr<<"Failed check "<<checks<<std::endl;std::exit(2);}};
    check(WebPageCapture::ValidUrl(QUrl("https://example.org/a?q=1")));
    check(WebPageCapture::ValidUrl(QUrl::fromLocalFile("C:/effects/example.html")));
    for(const char* address:{"javascript:alert(1)","data:text/html,x","file://server/share/a.html","ftp://example.org/a","https://user:password@example.org/","relative.html"}) check(!WebPageCapture::ValidUrl(QUrl(address)));
    check(WebPageCapture::ValidSize(800,600));check(!WebPageCapture::ValidSize(0,600));check(!WebPageCapture::ValidSize(4096,4097));
    QImage original(32,16,QImage::Format_RGB32);
    for(int y=0;y<16;++y) for(int x=0;x<32;++x) original.setPixel(x,y,qRgb(x*8,y*16,61));
    QByteArray bytes; QBuffer out(&bytes);out.open(QIODevice::WriteOnly);original.save(&out,"PNG");
    check(WebPageCapture::DecodePng(bytes,32,16)==original);
    check(WebPageCapture::DecodePng(bytes,16,32).isNull());check(WebPageCapture::DecodePng("bad",32,16).isNull());
    const auto plan=effect_canvas::BuildPlan(15,1,15,nullptr,false,{});
    check(plan.size()==15);check(qRed(effect_canvas::SamplePixel(original,plan.front()))<qRed(effect_canvas::SamplePixel(original,plan.back())));
    effect_canvas::Router router; ImageSink sink;
    check(router.Submit(&sink,0,original,1,100,0,0,{},false));
    check(sink.received && sink.received->Valid() && sink.received->width==32 && sink.received->height==16);
    check(room_image::SampleBGRA(*sink.received,sink.mapping,0.5,0.5)!=0xff000000u);
    router.SetRunning(false);auto previous=sink.received;
    check(router.Submit(&sink,0,original,2,0,0,0,{},false) && sink.received==previous);
    if(!app.arguments().contains("--browser")) {std::cout<<checks<<" parser/routing assertions passed\n";return 0;}
    QTemporaryDir folder;
    check(folder.isValid());
    const QString path=folder.filePath("gradient.html");
    QFile page(path);check(page.open(QIODevice::WriteOnly));
    page.write("<!doctype html><style>html,body{margin:0;width:100%;height:100%;overflow:hidden}body{background:linear-gradient(90deg,rgb(255,0,0),rgb(0,0,255))}</style>");page.close();
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost,0));
    QObject::connect(&server,&QTcpServer::newConnection,[&]
    {
        auto* socket=server.nextPendingConnection();
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket]
        {
            socket->readAll();
            const QByteArray body="<!doctype html><style>html,body{margin:0;width:100%;height:100%;background:rgb(0,255,0)}</style><body><script>function draw(t){document.body.style.background=(Math.floor(t/300)%2)?'rgb(0,0,255)':'rgb(0,255,0)';requestAnimationFrame(draw)}requestAnimationFrame(draw)</script>";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
            socket->disconnectFromHost();
        });
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    });
    // Environment creation is asynchronous: destruction must cancel it without
    // a dangling QObject callback or creating a controller later.
    auto* cancelled=new WebPageCapture();
    cancelled->Start(QUrl::fromLocalFile(path),160,100,5);
    cancelled->Stop(); delete cancelled;
    WebPageCapture capture;
    unsigned frames=0,phase=0;bool stopped=false,seen_green=false,seen_blue=false;
    QElapsedTimer heartbeat_clock;heartbeat_clock.start();
    qint64 previous_beat=0,max_gap=0;unsigned beats=0;
    QTimer heartbeat;heartbeat.setInterval(20);
    QObject::connect(&heartbeat,&QTimer::timeout,[&]
    {const auto now=heartbeat_clock.elapsed();max_gap=std::max(max_gap,now-previous_beat);previous_beat=now;++beats;});
    heartbeat.start();
    QObject::connect(&capture,&WebPageCapture::Status,[](const QString& text){std::cout<<text.toStdString()<<std::endl;});
    QObject::connect(&capture,&WebPageCapture::FrameReady,[&](const QImage& image)
    {
        check(!stopped);check(image.size()==QSize(800,600));
        const QRgb left=image.pixel(20,300),right=image.pixel(780,300);
        if(phase==0)
        {check(qRed(left)>200&&qBlue(left)<40);check(qBlue(right)>200&&qRed(right)<40);}
        else
        {
            seen_green|=qGreen(left)>250&&qRed(left)<5&&qBlue(left)<5;
            seen_blue|=qBlue(left)>250&&qRed(left)<5&&qGreen(left)<5;
            check(qRed(left)<5&&(qGreen(left)>250||qBlue(left)>250));check(left==right);
        }
        ++frames;
        if((phase==0&&frames==3)||(phase==1&&frames>=6&&seen_green&&seen_blue))
        {
            // Stop outside COM callback; drain a pending timer and prove no late frame.
            QTimer::singleShot(0,&app,[&]
            {
                stopped=true;capture.Stop();
                QTimer::singleShot(500,&app,[&]
                {
                    if(phase==0)
                    {
                        phase=1;frames=0;stopped=false;
                        capture.Start(QUrl("http://127.0.0.1:"+QString::number(server.serverPort())+"/green.html"),800,600,10);
                    }
                    else
                    {
                        check(beats>10);check(max_gap<500);
                        std::cout<<checks<<" assertions + "<<frames+3<<" WebView2 file/HTTP frames + JS animation + stop/restart/init-cancel passed; GUI heartbeat max gap "<<max_gap<<"ms\n";
                        app.exit(0);
                    }
                });
            });
        }
    });
    QTimer::singleShot(45000,&app,[&]{std::cerr<<"Browser test timeout; frames="<<frames<<std::endl;capture.Stop();app.exit(3);});
    capture.Start(QUrl::fromLocalFile(path),800,600,10);
    return app.exec();
}
