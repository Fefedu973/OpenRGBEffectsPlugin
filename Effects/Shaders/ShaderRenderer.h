/*---------------------------------------------------------*\
| ShaderRenderer.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Shader Renderer                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once
#include <atomic>
#include <array>

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
#include "MusicEnvelope.h"

typedef std::chrono::steady_clock::time_point TCount;

class ShaderRenderer : public QObject
{
    Q_OBJECT

public:
    explicit ShaderRenderer(QObject *parent = nullptr);
    ~ShaderRenderer();

    void Start();
    void Stop();
    void SetFPS(int);
    void Resize(int width, int height);
    void UpdateUniforms(float time, const float* audio);
    void UpdateCustomUniforms(const ShaderUniformMap& values);

    bool isRunning();

    ShaderProgram* Program();
    void SetProgram(ShaderProgram*);

private:
    Uniforms uniforms;
    std::array<float, 256> audio_values{};
    MusicEnvelope music_envelope;
    std::thread* thread = nullptr;
    void RendererThreadFunction();

    ShaderProgram* shader_program = nullptr;
    QOffscreenSurface* surface = nullptr;
    QOpenGLContext* context = nullptr;

    std::atomic<int> FPS{60};

    std::atomic<bool> running{false};

    std::mutex program_lock;

    static std::mutex context_lock;

signals:
    void Image(const QImage&);
    void Log(const QString&);
};
