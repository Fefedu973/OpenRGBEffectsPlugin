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
    {
        std::lock_guard<std::mutex> guard(program_lock);
        music_envelope.Reset();
        rhythm_envelope.Reset();
        uniforms.iRhythm = {}; uniforms.iOnset = {};
        uniforms.iMusic = {0.0f,0.0f,0.54f,0.0f};
        audio_values.fill(0.0f);
        uniforms.iAudio = nullptr;
    }

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
    if(width>0 && height>0 && width<=4096 && height<=4096){graph_width=unsigned(width);graph_height=unsigned(height);}
}

void ShaderRenderer::UpdateUniforms(float time, const float* audio, const room_audio::RhythmSnapshot* rhythm)
{
    std::lock_guard<std::mutex> guard(program_lock);
    uniforms.iTime = time;
    if(audio)
    {
        std::copy_n(audio, audio_values.size(), audio_values.begin());
        uniforms.iAudio = audio_values.data();
    }
    else uniforms.iAudio = nullptr;
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    uniforms.iMusic = music_envelope.Update(uniforms.iAudio, seconds);
    uniforms.iRhythm = {}; uniforms.iOnset = {};
    if(rhythm)
    {
        const auto result=rhythm_envelope.Update(*rhythm,seconds);
        uniforms.iRhythm=result.rhythm; uniforms.iOnset=result.transients;
        uniforms.iMusic[2]=result.hue; uniforms.iMusic[3]=result.rhythm[3];
    }
}

void ShaderRenderer::UpdateCustomUniforms(const ShaderUniformMap& values)
{
    std::lock_guard<std::mutex> guard(program_lock);
    uniforms.custom = values;
}

void ShaderRenderer::UpdateImage(unsigned slot,std::shared_ptr<const DynamicShaderImage> image)
{
    if(slot>=uniforms.images.size()) return;
    std::lock_guard<std::mutex> guard(program_lock);
    uniforms.images[slot]=std::move(image);
}

void ShaderRenderer::UpdateInputs(const ShaderUniformMap& values,const std::array<std::shared_ptr<const DynamicShaderImage>,4>& images)
{
    std::lock_guard<std::mutex> guard(program_lock);
    uniforms.custom=values;
    uniforms.images=images;
}

void ShaderRenderer::UpdateRenderGraph(std::shared_ptr<const ShaderRenderGraphFrame> frame)
{
    std::lock_guard<std::mutex> guard(program_lock);graph_frame=std::move(frame);
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

    std::unique_ptr<ShaderRenderGraphRunner> graph_runner;
    while(running)
    {
        TCount start = std::chrono::steady_clock::now();

        // DRAW program
        try
        {
            std::lock_guard<std::mutex> guard(program_lock);
            if(graph_frame)
            {
                if(!graph_runner)graph_runner=std::make_unique<ShaderRenderGraphRunner>();
                emit Image(graph_runner->Draw(*graph_frame,graph_width,graph_height));
            }
            else
            {
                graph_runner.reset();
                if(!shader_program->initialized)shader_program->Init();
                if(shader_program->recompile)emit Log(shader_program->Compile());
                shader_program->Draw(uniforms, context->functions());
                emit Image(shader_program->Image());
            }
        }
        catch(const std::exception& error)
        {
            emit Log(QStringLiteral("Shader rendering stopped: ")+QString::fromUtf8(error.what()));
            running=false;
        }
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
    graph_runner.reset();
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
