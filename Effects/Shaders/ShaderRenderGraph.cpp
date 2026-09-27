// SPDX-License-Identifier: GPL-2.0-or-later
#include "ShaderRenderGraph.h"
#include "ShaderCanvas.h"
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QVector2D>
#include <set>
#include <stdexcept>

class ShaderRenderGraphRunner::Impl
{
public:
    struct Target
    {
        std::unique_ptr<QOpenGLShaderProgram> shader;
        std::unique_ptr<QOpenGLFramebufferObject> buffer;
    };
    struct Input
    {
        ShaderPass upload{ShaderPass::DYNAMIC_IMAGE};
        ~Input(){upload.CleanupGL();}
    };
    QOpenGLFunctions* gl=QOpenGLContext::currentContext()->functions();
    std::shared_ptr<const ShaderRenderGraph> current;
    std::vector<Target> targets;
    std::map<std::string,std::unique_ptr<Input>> external;
    GLuint quad=0;
    Impl()
    {
        const GLfloat vertices[]={-1,-1,-1,1,1,-1,1,1};
        gl->glGenBuffers(1,&quad);gl->glBindBuffer(GL_ARRAY_BUFFER,quad);
        gl->glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        gl->glBindBuffer(GL_ARRAY_BUFFER,0);
    }
    ~Impl(){if(quad)gl->glDeleteBuffers(1,&quad);}
    void Compile(std::shared_ptr<const ShaderRenderGraph> graph)
    {
        if(!graph || graph->passes.empty() || graph->passes.size()>32)throw std::invalid_argument("Invalid GPU graph size");
        std::set<std::string> ids,all;
        std::uint64_t bytes=0;
        for(const auto& pass:graph->passes)
        {
            if(pass.id.empty() || !all.insert(pass.id).second || !ShaderCanvas::ValidSize(pass.width,pass.height))
                throw std::invalid_argument("Invalid GPU graph pass");
            bytes+=std::uint64_t(pass.width)*pass.height*8;
        }
        if(bytes>128ULL*1024*1024 || !all.count(graph->output))throw std::invalid_argument("GPU graph exceeds its memory budget or has no output");
        for(const auto& pass:graph->passes)
        {
            for(const auto& input:pass.inputs)if(all.count(input) && !ids.count(input))
                throw std::invalid_argument("GPU graph contains a forward reference or cycle");
            ids.insert(pass.id);
        }
        // Build transactionally: a failed shader never leaves half a graph live.
        std::vector<Target> next;
        for(const auto& pass:graph->passes)
        {
            Target target;target.shader=std::make_unique<QOpenGLShaderProgram>();
            const char* vertex="#version 130\nin vec2 vertices;void main(){gl_Position=vec4(vertices,0,1);}";
            const std::string fragment="#version 130\nuniform vec3 iResolution;\nuniform sampler2D iChannel0;\nuniform sampler2D iChannel1;\nuniform sampler2D iChannel2;\nuniform sampler2D iChannel3;\n"+pass.fragment+
                "\nvoid main(){vec4 color=vec4(0);mainImage(color,gl_FragCoord.xy);gl_FragColor=color;}\n";
            if(!target.shader->addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) ||
               !target.shader->addShaderFromSourceCode(QOpenGLShader::Fragment,fragment.c_str()))
                throw std::runtime_error("GPU graph shader compilation failed: "+target.shader->log().toStdString());
            target.shader->bindAttributeLocation("vertices",0);
            if(!target.shader->link())throw std::runtime_error("GPU graph linking failed: "+target.shader->log().toStdString());
            QOpenGLFramebufferObjectFormat format;format.setInternalTextureFormat(GL_RGBA16F);
            target.buffer=std::make_unique<QOpenGLFramebufferObject>(pass.width,pass.height,format);
            if(!target.buffer->isValid())throw std::runtime_error("GPU graph framebuffer allocation failed");
            gl->glBindTexture(GL_TEXTURE_2D,target.buffer->texture());
            gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);gl->glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            next.push_back(std::move(target));
        }
        targets=std::move(next);external.clear();current=std::move(graph);
    }
};

ShaderRenderGraphRunner::ShaderRenderGraphRunner():impl(std::make_unique<Impl>()){}
ShaderRenderGraphRunner::~ShaderRenderGraphRunner()=default;
QImage ShaderRenderGraphRunner::Draw(const ShaderRenderGraphFrame& frame,unsigned width,unsigned height)
{
    const auto black=[width,height]{QImage result(width,height,QImage::Format_RGB32);result.fill(Qt::black);return result;};
    if(!ShaderCanvas::ValidSize(width,height))throw std::invalid_argument("Invalid graph output dimensions");
    if(!frame.graph || std::chrono::steady_clock::now()>frame.expires || frame.images.size()>8)return black();
    for(const auto& input:frame.images)if(!input.second || !input.second->Usable())return black();
    if(impl->current!=frame.graph)impl->Compile(frame.graph);
    auto* gl=impl->gl;
    std::map<std::string,GLuint> textures;
    for(const auto& input:frame.images)
    {
        auto& external=impl->external[input.first];if(!external)external=std::make_unique<Impl::Input>();
        Uniforms uniforms;uniforms.images[0]=input.second;
        external->upload.Draw(uniforms,GL_TEXTURE0,gl);
        GLint binding=0;gl->glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);textures[input.first]=GLuint(binding);
    }
    const bool blend=gl->glIsEnabled(GL_BLEND),scissor=gl->glIsEnabled(GL_SCISSOR_TEST),depth=gl->glIsEnabled(GL_DEPTH_TEST);
    gl->glDisable(GL_BLEND);gl->glDisable(GL_SCISSOR_TEST);gl->glDisable(GL_DEPTH_TEST);
    QImage result;
    for(unsigned i=0;i<frame.graph->passes.size();++i)
    {
        const auto& pass=frame.graph->passes[i];auto& target=impl->targets[i];
        for(const auto& input:pass.inputs)if(!input.empty() && !textures.count(input))
        {if(blend)gl->glEnable(GL_BLEND);if(scissor)gl->glEnable(GL_SCISSOR_TEST);if(depth)gl->glEnable(GL_DEPTH_TEST);return black();}
        target.buffer->bind();target.shader->bind();
        target.shader->setUniformValue("iResolution",QVector3D(pass.width,pass.height,1));
        for(unsigned c=0;c<4;++c)
        {
            gl->glActiveTexture(GL_TEXTURE0+c);gl->glBindTexture(GL_TEXTURE_2D,pass.inputs[c].empty()?0:textures.at(pass.inputs[c]));
            target.shader->setUniformValue(("iChannel"+std::to_string(c)).c_str(),int(c));
        }
        for(const auto& item:pass.uniforms)
        {
            const auto& v=item.second.values;const auto* key=item.first.c_str();
            switch(item.second.components)
            {
            case 1:target.shader->setUniformValue(key,v[0]);break;
            case 2:target.shader->setUniformValue(key,QVector2D(v[0],v[1]));break;
            case 3:target.shader->setUniformValue(key,QVector3D(v[0],v[1],v[2]));break;
            case 4:target.shader->setUniformValue(key,QVector4D(v[0],v[1],v[2],v[3]));break;
            default:break;
            }
        }
        gl->glViewport(0,0,pass.width,pass.height);gl->glBindBuffer(GL_ARRAY_BUFFER,impl->quad);
        target.shader->enableAttributeArray(0);target.shader->setAttributeBuffer(0,GL_FLOAT,0,2);
        gl->glDrawArrays(GL_TRIANGLE_STRIP,0,4);gl->glBindBuffer(GL_ARRAY_BUFFER,0);
        target.shader->release();target.buffer->release();textures[pass.id]=target.buffer->texture();
        if(pass.id==frame.graph->output)result=target.buffer->toImage();
    }
    if(blend)gl->glEnable(GL_BLEND);if(scissor)gl->glEnable(GL_SCISSOR_TEST);if(depth)gl->glEnable(GL_DEPTH_TEST);
    return result.isNull()?black():result;
}
