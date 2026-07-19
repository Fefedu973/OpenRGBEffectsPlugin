/*---------------------------------------------------------*\
| ShaderRenderer.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Shader Renderer                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QObject>
#include <QImage>
#include <QDebug>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShader>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <thread>
#include <mutex>
#include "ShaderProgram.h"

typedef std::chrono::time_point<std::chrono::steady_clock,std::chrono::duration<long long, std::ratio<1,10000000000>>> TCount;

class ShaderRenderer : public QObject
{
    Q_OBJECT

public:
    explicit ShaderRenderer(QObject *parent = nullptr);
    ~ShaderRenderer();

    void Start();
    void Stop();
    void SetFPS(int);

    bool isRunning();

    ShaderProgram* Program();
    void SetProgram(ShaderProgram*);

    Uniforms uniforms;

private:
    std::thread* thread = nullptr;
    void RendererThreadFunction();

    ShaderProgram* shader_program = nullptr;
    QOffscreenSurface* surface = nullptr;
    QOpenGLContext* context = nullptr;

    int FPS = 60;

    bool running = false;

    std::chrono::steady_clock* clock;

    std::mutex program_lock;

    static std::mutex context_lock;

signals:
    void Image(const QImage&);
    void Log(const QString&);
};
