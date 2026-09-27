/*---------------------------------------------------------*\
| ShaderPass.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Shader Pass                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ShaderPass.h"
#include "RGBControllerInterface.h"
#include "ShaderCanvas.h"

#ifdef __linux__
#include <GL/gl.h>
#endif

namespace
{
QOpenGLFramebufferObject* CreateFramebuffer(int width,int height)
{
    // Qt allocates its texture on the currently active unit. Preserve a prior
    // pass's channel binding when a later pass resizes in the same frame.
    auto* gl=QOpenGLContext::currentContext()->functions();
    GLint active=0,binding=0;
    gl->glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    gl->glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
    auto* result=new QOpenGLFramebufferObject(width,height);
    gl->glActiveTexture(active);
    gl->glBindTexture(GL_TEXTURE_2D,binding);
    return result;
}
}

ShaderPass::ShaderPass(Type type)
{
    this->type = type;

    switch (type) {
    case BUFFER:
    {
        data.fragment_shader =
                "void mainImage( out vec4 fragColor, in vec2 fragCoord )\n"
                "{\n"
                    "  fragColor = vec4(1.0,0.0,0.0,1.0);\n"
                "}\n";
        break;
    }
    default: break;
    }
}

void ShaderPass::Init(int width, int height)
{
    if(type == DYNAMIC_IMAGE) return;
    Resize(width, height);

    program = new QOpenGLShaderProgram();
    program->link();

    // Create a VBO with a full-screen quad
    // Using a triangle strip (4 vertices) for the quad
    GLfloat quad_vertices[] =
    {
        -1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };

    QOpenGLFunctions* gl = QOpenGLContext::currentContext()->functions();
    gl->glGenBuffers(1, &vbo);
    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShaderPass::Resize(int width, int height)
{
    if(type == DYNAMIC_IMAGE) return;
    if(ShaderCanvas::ValidSize(data.width,data.height)) { width=data.width; height=data.height; }
    if(!ShaderCanvas::ValidSize(width,height)) return;
    this->width = width;
    this->height = height;

    if(fbo != nullptr)
    {
        delete fbo;
    }

    fbo = CreateFramebuffer(width, height);
    if(previous_fbo != nullptr)
    {
        delete previous_fbo;
        previous_fbo = nullptr;
    }
    ResetFeedback();
}

void ShaderPass::ResetFeedback()
{
    // The history texture is never the render target. A non-feedback pass
    // retains exactly one FBO, preserving its previous allocation behavior.
    if(type != BUFFER || !data.feedback)
    {
        delete previous_fbo;
        previous_fbo = nullptr;
        return;
    }
    if(!fbo) return;
    if(!previous_fbo) previous_fbo = CreateFramebuffer(width,height);
    auto* gl = QOpenGLContext::currentContext()->functions();
    GLfloat old_clear[4]; gl->glGetFloatv(GL_COLOR_CLEAR_VALUE,old_clear);
    const bool scissor = gl->glIsEnabled(GL_SCISSOR_TEST);
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glClearColor(0,0,0,0);
    for(auto* target : {fbo,previous_fbo})
    {
        target->bind(); gl->glClear(GL_COLOR_BUFFER_BIT); target->release();
    }
    gl->glClearColor(old_clear[0],old_clear[1],old_clear[2],old_clear[3]);
    if(scissor) gl->glEnable(GL_SCISSOR_TEST);
}

QString ShaderPass::Recompile(std::string version)
{
    if(type == DYNAMIC_IMAGE) return {};
    ResetFeedback();
    // re-link necessary??
    program->link();

    if(type == BUFFER)
    {
        program->removeAllShaders();

        program->addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    MakeVertexShader().c_str());

        program->addShaderFromSourceCode(
                    QOpenGLShader::Fragment,
                    MakeFragmentShader(version, data.fragment_shader).c_str());
    }
    else if(type == TEXTURE)
    {
        program->removeAllShaders();

        program->addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    MakeVertexShader().c_str());

        if(!data.texture_path.empty())
        {
            img = QImage(fbo->size(), QImage::Format_RGBA8888);
            img.load(QString::fromStdString(data.texture_path));
            texture = new QOpenGLTexture(
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                        img.flipped(Qt::Vertical),
#else
                        img.mirrored(false, true),
#endif
                        QOpenGLTexture::GenerateMipMaps
                        );
            texture->create();
        }
    }
    else if(type == AUDIO)
    {
        program->removeAllShaders();

        program->addShaderFromSourceCode(
                    QOpenGLShader::Vertex,
                    MakeVertexShader().c_str());

        img = QImage(512, 2, QImage::Format_RGBA8888);

        texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture->setSize(512, 2, 1);
        texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        texture->create();
    }

    return program->log();
}

void ShaderPass::Draw(const Uniforms& uniforms, GLenum unit, QOpenGLFunctions *gl)
{
    switch (type) {

    case BUFFER:
    {
        if(data.feedback && !previous_fbo) ResetFeedback();
        if(!data.feedback && previous_fbo) { delete previous_fbo; previous_fbo=nullptr; }
        QOpenGLFramebufferObject* target = previous_fbo ? previous_fbo : fbo;
        program->bind();
        // Units 0..3 are shader channels and 4 is the final output. History is
        // a separate source on unit 5; it is unbound before the FBOs swap.
        program->setUniformValue("iPreviousFrame",5);
        gl->glActiveTexture(GL_TEXTURE5);
        gl->glBindTexture(GL_TEXTURE_2D,previous_fbo ? fbo->texture() : 0);
        target->bind();

        program->setUniformValue("iTime", uniforms.iTime);

        if(uniforms.iAudio != nullptr)
        {
            program->setUniformValueArray("iAudio", uniforms.iAudio, 256, 1);
        }
        else
        {
            static const float silent_audio[256] = {};
            program->setUniformValueArray("iAudio", silent_audio, 256, 1);
        }
        program->setUniformValue("iMusic", QVector4D(uniforms.iMusic[0], uniforms.iMusic[1],
                                                     uniforms.iMusic[2], uniforms.iMusic[3]));
        program->setUniformValue("iRhythm", QVector4D(uniforms.iRhythm[0],uniforms.iRhythm[1],uniforms.iRhythm[2],uniforms.iRhythm[3]));
        program->setUniformValue("iOnset", QVector4D(uniforms.iOnset[0],uniforms.iOnset[1],uniforms.iOnset[2],uniforms.iOnset[3]));

        program->setUniformValue("iResolution", QVector3D(width, height, 1));
        program->setUniformValue("iMouse", QVector4D(0.,0.,0.,0.));
        program->setUniformValue("iChannel0", 0);
        program->setUniformValue("iChannel1", 1);
        program->setUniformValue("iChannel2", 2);
        program->setUniformValue("iChannel3", 3);
        const auto& screen=uniforms.images[0];
        const bool available=screen && screen->Usable();
        program->setUniformValue("iScreenAvailable",available ? 1.0f : 0.0f);
        // Re-evaluate immutable image leases on the renderer thread, even if
        // a producer stops publishing new uniforms or model results.
        QVector4D image_available;
        for(unsigned i=0;i<4;++i)image_available[int(i)]=uniforms.images[i]&&uniforms.images[i]->Usable()?1.f:0.f;
        program->setUniformValue("iImageAvailable",image_available);
        program->setUniformValue("iScreenResolution",available ? QVector3D(screen->Width(),screen->Height(),1) : QVector3D());

        for(const auto& entry : uniforms.custom)
        {
            const auto& v = entry.second;
            switch(v.components)
            {
            case 1: program->setUniformValue(entry.first.c_str(), v.values[0]); break;
            case 3: program->setUniformValue(entry.first.c_str(), QVector3D(v.values[0],v.values[1],v.values[2])); break;
            case 4: program->setUniformValue(entry.first.c_str(), QVector4D(v.values[0],v.values[1],v.values[2],v.values[3])); break;
            default: break;
            }
        }

        // Bind VBO and set vertex attribute
        gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        program->enableAttributeArray(0);
        program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

        gl->glViewport(0, 0, width, height);
        gl->glClear(GL_COLOR_BUFFER_BIT);

        // Draw full-screen quad using VBO
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

        // ...

        target->release();
        program->release();
        gl->glActiveTexture(GL_TEXTURE5);
        gl->glBindTexture(GL_TEXTURE_2D,0);
        if(previous_fbo) std::swap(fbo,previous_fbo);

        gl->glActiveTexture(unit);
        gl->glBindTexture(GL_TEXTURE_2D, fbo->texture());

        break;
    }

    case DYNAMIC_IMAGE:
    {
        DrawDynamicImage(uniforms,unit,gl);
        break;
    }

    case TEXTURE:
    {
        program->bind();
        fbo->bind();

        // Bind VBO and set vertex attribute
        gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        program->enableAttributeArray(0);
        program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

        gl->glViewport(0, 0, width, height);
        gl->glClear(GL_COLOR_BUFFER_BIT);

        // Draw full-screen quad using VBO
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

        fbo->release();
        program->release();

        if(!img.isNull())
        {
            gl->glActiveTexture(unit);
            gl->glBindTexture(GL_TEXTURE_2D, texture->textureId());

        }

        break;
    }

    case AUDIO:
    {
        program->bind();
        fbo->bind();

        // Bind VBO and set vertex attribute
        gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        program->enableAttributeArray(0);
        program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

        gl->glViewport(0, 0, width, height);
        gl->glClear(GL_COLOR_BUFFER_BIT);

        // Draw full-screen quad using VBO
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

        fbo->release();
        program->release();

        if(uniforms.iAudio != nullptr)
        {
            // the sound texture is 512x2
            // first row is frequency data (48Khz/4 in 512 texels, meaning 23 Hz per texel)
            // second row is the sound wave, one texel is one mono sample

            for (int i = 0; i < 256; i++) {

                unsigned int a = (int)(uniforms.iAudio[i] * 255);
                RGBColor c = ToRGBColor(a,a,a);
                img.setPixel(2*i, 0, c);
                img.setPixel(2*i+1, 0, c);

                a = (int)(uniforms.iAudio[i] + 1.0) * 127;
                c = ToRGBColor(a,a,a);
                img.setPixel(2*i, 1, c);
                img.setPixel(2*i+1, 1, c);
            }

            texture->destroy();
            texture->create();
            texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
            texture->setSize(512, 2, 1);
            texture->setFormat(QOpenGLTexture::R32F);
            texture->setData(img);

            gl->glActiveTexture(unit);
            gl->glBindTexture(GL_TEXTURE_2D, texture->textureId());
        }

        break;
    }

    default:
    {

        break;
    }

    }

}

std::string ShaderPass::MakeVertexShader()
{
    return  "attribute highp vec4 vertices;\n"
            "varying highp vec2 frag_coords;\n"
            "void main() {\n"
                "gl_Position = vec4(vertices.xy,0.0,1.0);\n"
            "}\n";
}

std::string ShaderPass::MakeFragmentShader(std::string pre_processor_version, std::string shader)
{
    std::string header =
            "#version " + pre_processor_version +  "\n"
            "#ifdef GL_ES\n"
            "precision highp float; \n"
            "#endif\n"
            "#define HW_PERFORMANCE 1 \n"
            "uniform vec3      iResolution;\n"
            "uniform vec4      iMouse;\n"
            "uniform float     iTime;\n"
            "uniform float     iAudio[256];\n"
            "uniform vec4      iMusic;\n"
            "uniform vec4      iRhythm;\n"
            "uniform vec4      iOnset;\n"
            "uniform sampler2D iChannel0;\n"
            "uniform sampler2D iChannel1;\n"
            "uniform sampler2D iChannel2;\n"
            "uniform sampler2D iChannel3;\n"
            "uniform sampler2D iPreviousFrame;\n"
            "uniform float iScreenAvailable;\n"
            "uniform vec3 iScreenResolution;\n"
        ;

    std::string includes =
            "\n"
            "vec3 HSVToRGB(vec3 c)\n"
                "{\n"
                    "return mix(vec3(1.),clamp((abs(fract(c.x+vec3(3,2,1)/3.)*6.-3.)-1.),0.,1.),c.y)*c.z;\n"
                "}\n"
        ;

    std::string footer =
            "\n"
            "void main( void )\n"
              "{\n"
                "vec4 color = vec4(0.0,0.0,0.0,1.0);\n"
                "mainImage(color, gl_FragCoord.xy);\n"
                "gl_FragColor = vec4(color.xyz,1.0);\n"
            "}\n"
        ;

    return
            header +
            includes +
            shader +
            footer ;
}

QImage ShaderPass::toImage()
{
    return fbo->toImage();
}

void ShaderPass::CleanupGL()
{
    if(dynamic_texture)
    {
        QOpenGLContext::currentContext()->functions()->glDeleteTextures(1,&dynamic_texture);
        dynamic_texture=0;
    }
    uploaded_image.reset(); dynamic_width=dynamic_height=0;
    if(vbo != 0)
    {
        QOpenGLFunctions* gl = QOpenGLContext::currentContext()->functions();
        gl->glDeleteBuffers(1, &vbo);
        vbo = 0;
    }

    if(texture != nullptr)
    {
        delete texture;
        texture = nullptr;
    }

    if(fbo != nullptr)
    {
        delete fbo;
        fbo = nullptr;
    }
    if(previous_fbo != nullptr)
    {
        delete previous_fbo;
        previous_fbo = nullptr;
    }

    if(program != nullptr)
    {
        delete program;
        program = nullptr;
    }
}

ShaderPass::~ShaderPass()
{
    // Note: GL resources (vbo, texture, fbo, program) should be cleaned up
    // via CleanupGL() while the GL context is still current.
    // The destructor only cleans up non-GL resources.
    // If CleanupGL() wasn't called, the GL resources will leak rather than
    // crash due to missing context.
}

ShaderPass* ShaderPass::Copy()
{
    ShaderPass* copy = new ShaderPass(type);

    copy->data = data;

    return copy;
}

ShaderPass::Type ShaderPass::GetType()
{
    return type;
}

ShaderPass* ShaderPass::FromJSON(json j)
{
    const int type=j.at("type").get<int>();
    if(type<TEXTURE || type>DYNAMIC_IMAGE)throw std::invalid_argument("Invalid shader pass type");
    const int width=j.value("width",0),height=j.value("height",0),slot=j.value("image_slot",0);
    if((width!=0 || height!=0) && !ShaderCanvas::ValidSize(width,height))throw std::invalid_argument("Invalid shader pass dimensions");
    if(slot<0 || slot>3)throw std::invalid_argument("Invalid shader image slot");
    auto pass=std::make_unique<ShaderPass>(Type(type));
    pass->data.fragment_shader = j.at("fragment_shader").get<std::string>();
    pass->data.texture_path = j.at("texture_path").get<std::string>();
    pass->data.feedback = j.value("feedback",false);
    pass->data.width = width; pass->data.height = height;pass->data.image_slot=unsigned(slot);
    return pass.release();
}

json ShaderPass::ToJSON()
{
    json j;

    j["type"] = type;
    j["fragment_shader"] = data.fragment_shader;
    j["texture_path"] = data.texture_path;
    j["feedback"] = data.feedback;
    j["width"] = data.width; j["height"] = data.height;
    j["image_slot"] = data.image_slot;

    return j;
}

void ShaderPass::DrawDynamicImage(const Uniforms& uniforms,GLenum unit,QOpenGLFunctions* gl)
{
    const auto input=uniforms.images[std::min(data.image_slot,3u)];
    const bool valid=input && input->Usable();
    gl->glActiveTexture(unit);
    if(!dynamic_texture) gl->glGenTextures(1,&dynamic_texture);
    gl->glBindTexture(GL_TEXTURE_2D,dynamic_texture);
    const unsigned w=valid ? input->Width() : 1, h=valid ? input->Height() : 1;
    const bool numeric=valid && bool(input->rgba32f);
    const bool resized=w!=dynamic_width || h!=dynamic_height || numeric!=dynamic_float;
    const bool changed=valid && (!uploaded_image || input->sequence!=uploaded_image->sequence ||
        input->source_revision!=uploaded_image->source_revision ||
        input->generation!=uploaded_image->generation || input->image.cacheKey()!=uploaded_image->image.cacheKey() ||
        input->rgba32f!=uploaded_image->rgba32f);
    if(!resized && !changed && (valid || !uploaded_image)) return;

    gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,numeric ? GL_NEAREST : GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,numeric ? GL_NEAREST : GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    GLint alignment=4; gl->glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); gl->glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    QImage packed;
    const unsigned char black[4]={0,0,0,255};
    const void* pixels=black;
    if(numeric) pixels=input->rgba32f->data();
    else if(valid)
    {
        packed=input->image.convertToFormat(QImage::Format_RGBA8888);
        if(packed.bytesPerLine()!=int(w*4)) packed=packed.copy();
        pixels=packed.constBits();
    }
    if(resized) gl->glTexImage2D(GL_TEXTURE_2D,0,numeric ? GL_RGBA32F : GL_RGBA,int(w),int(h),0,GL_RGBA,numeric ? GL_FLOAT : GL_UNSIGNED_BYTE,pixels);
    else gl->glTexSubImage2D(GL_TEXTURE_2D,0,0,0,int(w),int(h),GL_RGBA,numeric ? GL_FLOAT : GL_UNSIGNED_BYTE,pixels);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
    dynamic_width=w; dynamic_height=h; dynamic_float=numeric;
    uploaded_image=valid ? input : nullptr;
    ++image_uploads;
}
