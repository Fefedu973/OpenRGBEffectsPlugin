/*---------------------------------------------------------*\
| ShaderRenderer.cpp                                        |
|                                                           |
|   OpenRGB Effects Plugin Shader Renderer                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "OpenRGBEffectsPlugin.h"
#include "ShaderRenderer.h"
#include <algorithm>

std::mutex ShaderRenderer::context_lock;

ShaderRenderer::ShaderRenderer(QObject *parent) :
    QObject(parent),
    shader_program(new ShaderProgram)
{}

void ShaderRenderer::Start()
{
    if(running)
    {
        return;
    }

    shader_program->initialized = false;
    shader_program->recompile = true;

    running = true;
    thread = new std::thread(&ShaderRenderer::RendererThreadFunction, this);
}

void ShaderRenderer::Stop()
{
    if(thread != nullptr)
    {
        running = false;
        thread->join();
        delete thread;
        thread = nullptr;
    }
}

void ShaderRenderer::SetFPS(int value)
{
    FPS = std::clamp(value,1,240);
}

void ShaderRenderer::Resize(int width, int height)
{
    std::lock_guard<std::mutex> guard(program_lock);
    shader_program->Resize(width,height);
}

void ShaderRenderer::UpdateUniforms(float time, const float* audio)
{
    std::lock_guard<std::mutex> guard(program_lock);
    uniforms.iTime = time;
    if(audio)
    {
        std::copy_n(audio, audio_values.size(), audio_values.begin());
        uniforms.iAudio = audio_values.data();
    }
    else uniforms.iAudio = nullptr;
}

void ShaderRenderer::RendererThreadFunction()
{
    context_lock.lock();

    surface = new QOffscreenSurface();
    surface->create();

    context = new QOpenGLContext();
    context->setFormat(surface->format());
    if(!context->create() || !context->makeCurrent(surface))
    {
        emit Log(QStringLiteral("Unable to create the OpenGL rendering context."));
        delete context;
        delete surface;
        context = nullptr;
        surface = nullptr;
        running = false;
        context_lock.unlock();
        return;
    }

    QOpenGLFunctions* functions = context->functions();

    std::string vendor = reinterpret_cast<const char*>(functions->glGetString(GL_VENDOR));
    std::string renderer = reinterpret_cast<const char*>(functions->glGetString(GL_RENDERER));
    std::string version = reinterpret_cast<const char*>(functions->glGetString(GL_VERSION));

    LOG_VERBOSE("[OpenRGBEffectsPlugin] OpenGL vendor: %s, renderer: %s, version: %s", vendor.c_str(), renderer.c_str(), version.c_str());

    surface->setFormat(context->format());

    context_lock.unlock();

    while(running)
    {
        TCount start = std::chrono::steady_clock::now();

        // DRAW program
        program_lock.lock();

        if(!shader_program->initialized)
        {
            shader_program->Init();
        }

        if(shader_program->recompile)
        {
            emit Log(shader_program->Compile());
        }

        shader_program->Draw(uniforms, context->functions());

        emit Image(shader_program->Image());

        program_lock.unlock();
        // .....

        TCount end = std::chrono::steady_clock::now();

        int duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        int FPSDelay = 1000000 / FPS.load();
        int delta = FPSDelay - duration;

        if(delta > 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(delta));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Clean up GL resources while the context is still current
    program_lock.lock();
    if(shader_program != nullptr)
    {
        shader_program->CleanupGL();
    }
    program_lock.unlock();

    delete surface;
    delete context;
}


ShaderRenderer::~ShaderRenderer()
{
    Stop();

    if(shader_program != nullptr)
    {
        delete shader_program;
    }
}

ShaderProgram* ShaderRenderer::Program()
{
    return shader_program;
}

bool ShaderRenderer::isRunning()
{
    return running;
}

void ShaderRenderer::SetProgram(ShaderProgram* program)
{
    program_lock.lock();
    shader_program = program;
    shader_program->recompile = true;
    program_lock.unlock();
}
