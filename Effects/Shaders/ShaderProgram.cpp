/*---------------------------------------------------------*\
| ShaderProgram.cpp                                         |
|                                                           |
|   OpenRGB Effects Plugin Shader Program                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ShaderProgram.h"
#include "ShaderCanvas.h"

ShaderProgram::ShaderProgram()
{    
    main_pass = new ShaderPass(ShaderPass::BUFFER);
}

ShaderProgram::~ShaderProgram()
{
    for(auto* pass:passes) delete pass;
    passes.clear();

    delete main_pass;
}

void ShaderProgram::Init()
{
    if(passes.size()>4) throw std::invalid_argument("A shader program supports at most four input passes");
    for(ShaderPass* pass: passes)
    {
        pass->Init(width, height);
    }

    main_pass->Init(width, height);

    initialized = true;
}

QString ShaderProgram::Compile()
{
    QString log = main_pass->Recompile(version);

    for(ShaderPass* pass: passes)
    {
        log += pass->Recompile(version);
    }

    recompile = false;

    return log;
}

void ShaderProgram::Draw(const Uniforms& uniforms, QOpenGLFunctions* gl)
{
    GLuint units[4] = {GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3};
    int i = 0;

    for(ShaderPass* pass: passes)
    {
        if(resize)
        {
            pass->Resize(width, height);
        }

        pass->Draw(uniforms, units[i++], gl);
    }

    if(resize)
    {
        main_pass->Resize(width, height);
    }

    main_pass->Draw(uniforms, GL_TEXTURE4, gl);

    resize = false;
}

void ShaderProgram::CleanupGL()
{
    for(ShaderPass* pass: passes)
    {
        pass->CleanupGL();
    }

    main_pass->CleanupGL();
}

QImage ShaderProgram::Image()
{
    return main_pass->toImage();
}

void ShaderProgram::Resize(int width, int height)
{
    if(width <= 0 || height <= 0 || !ShaderCanvas::ValidSize(width,height)) return;
    this->width = width;
    this->height = height;

    resize = true;
}

void ShaderProgram::SetVersion(std::string version)
{
    this->version = version;
}

std::string ShaderProgram::GetVersion()
{
    return version;
}

ShaderProgram* ShaderProgram::Copy()
{
    ShaderProgram* copy = new ShaderProgram();
    delete copy->main_pass;
    copy->main_pass = main_pass->Copy();

    copy->version = version;
    copy->width = width;
    copy->height = height;

    for(ShaderPass* pass: passes)
    {
        copy->passes.push_back(pass->Copy());
    }

    return copy;

}

ShaderProgram* ShaderProgram::FromJSON(json j)
{
    if(!j.at("passes").is_array() || j.at("passes").size()>4)throw std::invalid_argument("A shader program supports at most four input passes");
    auto main=std::unique_ptr<ShaderPass>(ShaderPass::FromJSON(j.at("main_pass")));
    if(main->GetType()!=ShaderPass::BUFFER)throw std::invalid_argument("Shader output must be a buffer pass");
    auto prog=std::make_unique<ShaderProgram>();
    delete prog->main_pass;prog->main_pass=main.release();

    prog->version = j["version"];
    prog->Resize(j["width"], j["height"]);

    for(json pass_json: j["passes"])
    {
        prog->passes.push_back(ShaderPass::FromJSON(pass_json));
    }

    return prog.release();
}

json ShaderProgram::ToJSON()
{
    json j;

    j["main_pass"] = main_pass->ToJSON();
    j["version"] = version;
    j["width"] = width;
    j["height"] = height;

    std::vector<json> passes_json;

    for(ShaderPass* pass: passes)
    {
        passes_json.push_back(pass->ToJSON());
    }

    j["passes"] = passes_json;

    return j;
}
