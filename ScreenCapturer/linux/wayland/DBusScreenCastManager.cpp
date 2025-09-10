#include "DBusScreenCastManager.h"
#include <stdio.h>
#include <math.h>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QDBusArgument>
#include <QDBusUnixFileDescriptor>
#include <QDBusMetaType>
#include <QMetaType>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>
#include <fcntl.h>
#include <spa/debug/format.h>
#include <spa/debug/types.h>
#include <spa/utils/result.h>
#include <spa/pod/dynamic.h>

// docs https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.portal.ScreenCast.xml

unsigned long DBusScreenCastManager::RequestTokenCounter = 0;
unsigned long DBusScreenCastManager::SessionTokenCounter = 0;

DBusScreenCastManager::DBusScreenCastManager(QObject* parent) : QObject(parent){}

DBusScreenCastManager::~DBusScreenCastManager()
{
    Stop();
    Clear();
}

bool DBusScreenCastManager::IsSessionCreated()
{
    return session_created;
}

bool DBusScreenCastManager::IsSourceSelected()
{
    return sources_selected;
}

bool DBusScreenCastManager::IsStarted()
{
    return started;
}

bool DBusScreenCastManager::IsStreamOpened()
{
    return stream_opened;
}

void DBusScreenCastManager::SetToken(QString token)
{
    restore_token = token;
}

void DBusScreenCastManager::OpenStream()
{
    if(!session_created)
    {
        CreateSession(restore_token, true);
        return;
    }

    if(streams.size() == 1)
    {
        OpenPipeWireRemote(streams[0]);
    }
    else
    {
        qDebug() << "[DBusScreenCastManager] No available streams yet";
    }
}

void DBusScreenCastManager::CloseSession()
{
    if(!session_handle.isEmpty())
    {
        QDBusInterface i = QDBusInterface(
            "org.gnome.Shell",
            "/org/gnome/Shell/Extensions/WindowsExt",
            "org.freedesktop.portal.Session", bus, nullptr);

        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            session_handle,
            QStringLiteral("org.freedesktop.portal.Session"),
            QStringLiteral("Close")
            );

        QDBusMessage reply = bus.call(message);
        qDebug() << "close reply " << reply;

        session_handle.clear();
        session_created = false;
    }
}


void DBusScreenCastManager::Stop()
{
    CloseSession();

    if(watcher != nullptr)
    {
        delete watcher;
        watcher = nullptr;
    }

    started = false;
}

void DBusScreenCastManager::Clear()
{
    CloseSession();
    restore_token.clear();
    streams.clear();
}

// Dbus stuff
DBusPathToken DBusScreenCastManager::NewPath(QString type)
{
    QString token = "u" + QString::number(++RequestTokenCounter);
    QString path = "/org/freedesktop/portal/desktop/" + type + "/" + i.objectName() + "/" + token;
    return {token, path};
}

DBusPathToken DBusScreenCastManager::NewRequestPath()
{
    return NewPath("request");
}

DBusPathToken DBusScreenCastManager::NewSessionPath()
{
    return NewPath("session");
}

void DBusScreenCastManager::CreateSession(const QString& restore_token, bool auto_open_stream)
{
    qDebug() << "[DBusScreenCastManager] ###############";
    qDebug() << "[DBusScreenCastManager] CreateSession() restore_token:" << (restore_token.isEmpty() ? "(empty)" : restore_token);

    this->auto_open_stream = auto_open_stream;
    this->restore_token = restore_token;

    DBusPathToken new_session_path = NewSessionPath();
    DBusPathToken new_request_path = NewRequestPath();

    QVariantMap options;
    options["handle_token"] = new_request_path.token;
    options["session_handle_token"] = new_session_path.token;

    QDBusReply<QDBusObjectPath> repl = i.call("CreateSession", options);

    if(repl.isValid())
    {
        bool con = bus.connect("", repl.value().path(), "org.freedesktop.portal.Request", "Response", this, SLOT(OnSessionCreated(uint, QVariantMap)));
        qDebug() << "con=" << con;
    }
    else
    {
        qDebug() << repl.error() << bus.lastError();
        emit OnError(repl.error(), "Cannot call CreateSession");
    }
}

void DBusScreenCastManager::OnSessionCreated(uint responseCode, QVariantMap results)
{
    if(responseCode == 0)
    {
        session_created = true;
        session_handle = results["session_handle"].toString();
        qDebug() << "[DBusScreenCastManager] Session created with session handle = " << session_handle;
        SelectSources();
    }
    else
    {
        qDebug() << bus.lastError();
        emit OnError(bus.lastError(), "Cannot create session");
    }
}

void DBusScreenCastManager::SelectSources()
{
    QVariantMap options;
    DBusPathToken new_request_path = NewRequestPath();

    options["handle_token"] = new_request_path.token;
    options["multiple"] = false;
    options["types"] = quint32(1|2);
    options["persist_mode"] = quint32(2);

    if(!restore_token.isEmpty())
    {
        options["restore_token"] = restore_token;
    }

    QDBusReply<QDBusObjectPath> repl = i.call("SelectSources", QDBusObjectPath(session_handle), options);

    if(repl.isValid())
    {
        bus.connect("", repl.value().path(), "org.freedesktop.portal.Request", "Response", this, SLOT(OnSourceSelected(uint, QVariantMap)));
    }
    else
    {
        qDebug() << "SelectSources() Something is wrong: " << repl.error();
    }
}

void DBusScreenCastManager::OnSourceSelected(uint responseCode, QVariantMap /*results*/) {
    if(responseCode == 0)
    {
        sources_selected = true;

        qDebug() << "[DBusScreenCastManager] OnSourceSelected";
        Start();
    }
    else
    {
        qDebug() << "OnSourceSelected() response code " << responseCode;
    }
}

void DBusScreenCastManager::Start()
{
    QVariantMap options;
    DBusPathToken new_request_path = NewRequestPath();
    options["handle_token"] = new_request_path.token;

    QDBusReply<QDBusObjectPath> repl = i.call("Start", QDBusObjectPath(session_handle), "", options);

    if(repl.isValid())
    {
        bus.connect("", repl.value().path(), "org.freedesktop.portal.Request", "Response", this, SLOT(OnStarted(uint, QVariantMap)));
    }
    else
    {
        qDebug() << "Start() Something is wrong: " << repl.error();
    }
}

void DBusScreenCastManager::OnStarted(uint responseCode, QVariantMap results) {
    streams.clear();

    if(responseCode == 0)
    {
        qDebug() << "[DBusScreenCastManager] OnStarted";
        started = true;

        qDebug() << "[DBusScreenCastManager] emiting token";
        restore_token = results["restore_token"].toString();
        emit OnRestoreTokenAcquired(restore_token);                

        streams = qdbus_cast<PipeWireStreamInfoList>(results["streams"].value<QDBusArgument>()) ;

        qDebug() << "[DBusScreenCastManager] Streams infos:";

        for(PipeWireStreamInfo stream: streams)
        {
            qDebug() << "----------";
            qDebug() << "Node id" << stream.node_id;
            qDebug() << "Width" << stream.width << "Height" << stream.height;
            qDebug() << "X" << stream.x << "y" << stream.y;
            qDebug() << "Source type " << stream.source_type;
            qDebug() << "----------";
        }

        // ready
        if(auto_open_stream)
        {
            OpenStream();
        }
    }
    else
    {
        qDebug() << "[DBusScreenCastManager] OnStarted() response code " << responseCode;
    }

}

void DBusScreenCastManager::OpenPipeWireRemote(const PipeWireStreamInfo& stream_info)
{
    this->stream_info = stream_info;
    QVariantMap options;

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                          QStringLiteral("/org/freedesktop/portal/desktop"),
                                                          QStringLiteral("org.freedesktop.portal.ScreenCast"),
                                                          QStringLiteral("OpenPipeWireRemote"));

    message <<  QDBusObjectPath(session_handle) << options;

    if(watcher != nullptr)
    {
        delete watcher;
        watcher = nullptr;
    }

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);

    watcher = new QDBusPendingCallWatcher(pendingCall);

    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(OnCallFinished(QDBusPendingCallWatcher*)));
}

void DBusScreenCastManager::OnCallFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QDBusUnixFileDescriptor> reply = *watcher;

    if (reply.isError())
    {
        qDebug() << "[DBusScreenCastManager] OpenPipeWireRemote() Couldn't get reply";
        qDebug() << "[DBusScreenCastManager] Error: " << reply.error().message();
    }
    else
    {
        stream_opened = true;
        int fd = reply.value().fileDescriptor();
        qDebug() << "[DBusScreenCastManager] OpenPipeWireRemote fd available:" << fd;

        emit OnPipeWireStreamOpened(stream_info.node_id, fd, stream_info.width , stream_info.height);
    }
}

