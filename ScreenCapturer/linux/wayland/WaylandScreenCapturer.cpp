#include "WaylandScreenCapturer.h"

WaylandScreenCapturer::WaylandScreenCapturer(QObject* parent): ScreenCapturer(parent)
{
    dbus_manager = new DBusScreenCastManager(this);
    capturer = new PipeWireCapturer(this);

    connect(capturer, &PipeWireCapturer::NewImage, this, [&](const QImage& img){
        emit OnImage(img);
    });

    connect(dbus_manager, &DBusScreenCastManager::OnRestoreTokenAcquired,[&](const QString& token){
        emit OnRestoreTokenProvided(token);
    });

    connect(dbus_manager, &DBusScreenCastManager::OnPipeWireStreamOpened,[&](quint32 node_id, int fd, unsigned int width, unsigned int height){
        capturer->StartStream(node_id, fd, width, height, framerate);
    });

    connect(dbus_manager, &DBusScreenCastManager::OnError,[=](const QDBusError& /*err*/, const QString& message){
        emit OnError(Other, message);
    });
}

WaylandScreenCapturer::~WaylandScreenCapturer()
{
    delete capturer;
    delete dbus_manager;
}

void WaylandScreenCapturer::Start()
{
    dbus_manager->OpenStream();
}

void WaylandScreenCapturer::Stop()
{    
    capturer->StopStream();
    dbus_manager->Stop();
}

void WaylandScreenCapturer::Init(const QString& restore_token, bool auto_start)
{
    qDebug() << "[WaylandScreenCapturer] init" << restore_token << auto_start;

    if(dbus_manager->IsStreamOpened())
    {
        dbus_manager->Stop();
        dbus_manager->Clear();
        capturer->StopStream();
    }

    dbus_manager->CreateSession(restore_token, auto_start);
}

void WaylandScreenCapturer::SetToken(const QString& restore_token)
{
    dbus_manager->SetToken(restore_token);
}

void WaylandScreenCapturer::SetScreen(int)
{
    // not supported
}
