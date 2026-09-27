/*---------------------------------------------------------*\
| ShaderPass.h                                              |
|                                                           |
|   OpenRGB Effects Plugin Shader Pass                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QDebug>
#include <QImage>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShader>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <thread>
#include <array>
#include <map>
#include <nlohmann/json.hpp>
#include "ShaderPassData.h"

using json = nlohmann::json;

struct ShaderUniform
{
    std::array<float,4> values{};
    int components = 1;
};
using ShaderUniformMap = std::map<std::string, ShaderUniform>;

struct Uniforms
{
    float iTime = 0.f;
    float* iAudio = nullptr;
    std::array<float,4> iMusic{0.0f,0.0f,0.54f,0.0f};
    std::array<float,4> iRhythm{}, iOnset{};
    ShaderUniformMap custom;
};

class ShaderPass
{
public:

    enum Type {
        TEXTURE,
        AUDIO,
        BUFFER
    };

    ShaderPass(Type);
    ~ShaderPass();

    // Must be called in an OpenGL context thread
    void Init(int, int);
    QString Recompile(std::string);
    void Draw(const Uniforms&, GLenum, QOpenGLFunctions*);
    void Resize(int,int);
    QImage toImage();

    // Cleanup OpenGL resources. Must be called from the renderer thread
    // while the GL context is still current.
    void CleanupGL();

    ShaderPass* Copy();

    ShaderPass::Type GetType();

    ShaderPassData data;

    static ShaderPass* FromJSON(json);
    json ToJSON();


private:
    QOpenGLFramebufferObject* fbo = nullptr;
    QOpenGLShaderProgram* program = nullptr;

    std::string MakeVertexShader();
    std::string MakeFragmentShader(std::string,std::string);

    int width;
    int height;
    Type type;

    QImage img;

    QOpenGLTexture* texture = nullptr;

    GLuint vbo = 0;
};
