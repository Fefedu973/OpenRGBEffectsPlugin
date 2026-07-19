/*---------------------------------------------------------*\
| PipeWireCapturer.h                                        |
|                                                           |
|   OpenRGB Effects Plugin PipeWire Capturer                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QImage>
#include <QObject>
#include <QtGlobal>
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <sys/mman.h>

class PipeWireCapturer : public QObject
{
    Q_OBJECT

public:
    PipeWireCapturer(QObject* parent);
    ~PipeWireCapturer();

    void StartStream(quint32 id, int fd, unsigned int width, unsigned int height, unsigned int framerate);
    void StopStream();

signals:
    void NewImage(const QImage&);

private:
    struct pw_context* context = nullptr;
    struct pw_core* core = nullptr;
    struct pw_stream* stream = nullptr;
    struct pw_thread_loop* main_loop = nullptr;
    spa_hook core_listener;
    spa_hook stream_listener;
    pw_core_events core_events = {};
    pw_stream_events stream_events = {};
    struct spa_video_info_raw video_format;
    quint32 node_id = 0;
    quint32 fd = -1;
    unsigned int width;
    unsigned int height;
    unsigned int framerate;

    pw_stream* CreateReceivingStream();
    void HandleBuffer(pw_buffer* buffer);

    static void OnCoreError(void* data,uint32_t id, int seq, int res, const char* message);
    static void OnStreamParamChanged(void* data, uint32_t id, const struct spa_pod* format);
    static void OnStreamStateChanged(void* data, pw_stream_state old_state, pw_stream_state state, const char* error_message);
    static void OnStreamProcess(void* data);
    static void OnNewBuffer(void* data, uint32_t id);
};


class ScopedBuf {
public:
    ScopedBuf() {}
    ScopedBuf(uint8_t* map, int map_size, int fd)
        : map_(map), map_size_(map_size), fd_(fd) {}
    ~ScopedBuf() {
        if (map_ != MAP_FAILED) {
            munmap(map_, map_size_);
        }
    }
    operator bool() { return map_ != MAP_FAILED; }
    void initialize(uint8_t* map, int map_size, int fd) {
        map_ = map;
        map_size_ = map_size;
        fd_ = fd;
    }
    uint8_t* get() { return map_; }
protected:
    uint8_t* map_ = static_cast<uint8_t*>(MAP_FAILED);
    int map_size_;
    int fd_;
};
