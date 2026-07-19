/*---------------------------------------------------------*\
| PipeWireCapturer.cpp                                      |
|                                                           |
|   OpenRGB Effects Plugin PipeWire Capturer                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "PipeWireCapturer.h"
#include <QDebug>
#include <unistd.h>
#include <cstring>
#include "SpaPodUtils.h"

// doc https://webrtc.googlesource.com/src/+/refs/heads/main/modules/desktop_capture/linux/wayland/

PipeWireCapturer::PipeWireCapturer(QObject* parent) :  QObject(parent)  {
    pw_init(NULL,NULL);
    qDebug() << "Compiled with libpipewire " << pw_get_headers_version();
    qDebug() << "Linked with libpipewire " << pw_get_library_version();
}

PipeWireCapturer::~PipeWireCapturer()
{
    StopStream();
}

void PipeWireCapturer::OnCoreError(void* /*data*/, uint32_t /*id*/, int /*seq*/, int /*res*/, const char* message)
{
    qDebug() << "OnCoreError " << message;
}

void PipeWireCapturer::OnStreamStateChanged(void* /*data*/, pw_stream_state old_state, pw_stream_state state, const char* /*error_message*/) {

    qDebug() << "stream state changed from " << old_state << " to " << state;
}

void PipeWireCapturer::OnStreamParamChanged(void* data, uint32_t id, const struct spa_pod* format) {
    PipeWireCapturer* _this = static_cast<PipeWireCapturer*>(data);

    qDebug() << "PipeWire stream format changed.";

    if (!format || id != SPA_PARAM_Format)
    {
        return;
    }

    spa_format_video_raw_parse(format, &_this->video_format);

    qDebug() << "Format " << _this->video_format.format;
    qDebug() << "Width " << _this->video_format.size.width;
    qDebug() << "Height " << _this->video_format.size.height;
}

// static
void PipeWireCapturer::OnStreamProcess(void* data) {

    PipeWireCapturer* _this = static_cast<PipeWireCapturer*>(data);

    struct pw_buffer* next_buffer;
    struct pw_buffer* buffer = nullptr;
    next_buffer = pw_stream_dequeue_buffer(_this->stream);

    while (next_buffer)
    {
        buffer = next_buffer;
        next_buffer = pw_stream_dequeue_buffer(_this->stream);

        if (next_buffer)
        {
            pw_stream_queue_buffer(_this->stream, buffer);
        }
    }

    if (!buffer)
    {
        return;
    }

    _this->HandleBuffer(buffer);

    pw_stream_queue_buffer(_this->stream, buffer);
}

void PipeWireCapturer::HandleBuffer(pw_buffer* buffer) {
    spa_buffer* spa_buffer = buffer->buffer;
    ScopedBuf map;
    std::unique_ptr<uint8_t[]> src_unique_ptr;
    uint8_t* src = nullptr;

    if (spa_buffer->datas[0].chunk->size == 0)
    {
        qDebug() << "Failed to get video stream: Zero size.";
        return;
    }

    if (spa_buffer->datas[0].type == SPA_DATA_MemFd)
    {
        map.initialize(static_cast<uint8_t*>(
                           mmap(nullptr,
                                spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
                                PROT_READ, MAP_PRIVATE, spa_buffer->datas[0].fd, 0)),
                       spa_buffer->datas[0].maxsize + spa_buffer->datas[0].mapoffset,
                       spa_buffer->datas[0].fd);

        if (!map)
        {
            qDebug() << "Failed to mmap the memory: " << std::strerror(errno);
            return;
        }

        src = SPA_MEMBER(map.get(), spa_buffer->datas[0].mapoffset, uint8_t);
    }

    if (!src)
    {
        return;
    }

    unsigned int height = video_format.size.height;
    unsigned int width = video_format.size.width;

    QImage img(src, width, height, QImage::Format_RGBA8888);

    switch(video_format.format){

    case SPA_VIDEO_FORMAT_BGRA :
    case SPA_VIDEO_FORMAT_BGRx :
        emit NewImage(std::move(img).rgbSwapped());
        break;

    default:
        emit NewImage(img.copy());
        break;
    }
}

pw_stream* PipeWireCapturer::CreateReceivingStream() {
    pw_properties* reuseProps = pw_properties_new_string("pipewire.client.reuse=1");
    auto stream = pw_stream_new(core, "webrtc-consume-stream", reuseProps);
    uint8_t buffer[2048] = {};

    spa_pod_builder builder = spa_pod_builder{buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0};
    std::vector<const spa_pod*> params;

    struct spa_rectangle resolution = SPA_RECTANGLE(width, height);

    params.push_back(SpaPodUtils::CreateFormatOptions(&builder, &resolution, framerate));
    pw_stream_add_listener(stream, &stream_listener, &stream_events, this);

    if (pw_stream_connect(stream, PW_DIRECTION_INPUT, node_id,
                          (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params.data(), params.size()) != 0)
    {
        qDebug() << "Could not connect receiving stream.";
        return nullptr;
    }

    return stream;
}
void PipeWireCapturer::StartStream(quint32 id, int fd, unsigned int width, unsigned int height, unsigned int framerate){

    qDebug() << "PipeWireCapturer::StartStream";

    this->width = width;
    this->height = height;
    this->framerate = framerate;

    node_id = id;
    this->fd = fd;

    main_loop = pw_thread_loop_new("pipewire-main-loop", nullptr);
    pw_thread_loop_lock(main_loop);

    context = pw_context_new(pw_thread_loop_get_loop(main_loop), nullptr, 0);

    if (!context)
    {
        qDebug() << "Failed to create PipeWire context";
        return;
    }

    core = pw_context_connect_fd(context, fd, nullptr, 0);

    if (!core)
    {
        qDebug() << "Failed to connect PipeWire context";
        return;
    }

    core_events.version = PW_VERSION_CORE_EVENTS;
    core_events.error = &OnCoreError;
    stream_events.version = PW_VERSION_STREAM_EVENTS;
    stream_events.state_changed = &OnStreamStateChanged;
    stream_events.param_changed = &OnStreamParamChanged;
    stream_events.process = &OnStreamProcess;

    pw_core_add_listener(core, &core_listener, &core_events, this);

    stream = CreateReceivingStream();

    if (!stream)
    {
        qDebug() << "Failed to create PipeWire stream";
        return;
    }

    if (pw_thread_loop_start(main_loop) < 0)
    {
        qDebug()  << "Failed to start main PipeWire loop";
        return;
    }

    pw_thread_loop_unlock(main_loop);

    qDebug() << "main loop unlocked";

}

void PipeWireCapturer::StopStream()
{
    if(main_loop)
    {
        qDebug() << "pw_thread_loop_stop";
        pw_thread_loop_wait(main_loop);
        pw_thread_loop_stop(main_loop);        
    }

    if(stream)
    {
        qDebug() << "pw_stream_disconnect";

        pw_stream_disconnect(stream);
        qDebug() << "pw_stream_destroy";

        pw_stream_destroy(stream);
        stream = nullptr;
    }

    if(core)
    {
        qDebug() << "pw_core_disconnect";

        pw_core_disconnect(core);
        core = nullptr;
    }

    if(context)
    {
        qDebug() << "pw_context_destroy";

        pw_context_destroy(context);
        context = nullptr;
    }

    if(main_loop)
    {
        qDebug() << "pw_thread_loop_destroy";

        pw_thread_loop_destroy(main_loop);
        main_loop = nullptr;
    }

    if(fd > 0)
    {
        qDebug() << "close(fd)";

        close(fd);
        fd = -1;
    }
}
