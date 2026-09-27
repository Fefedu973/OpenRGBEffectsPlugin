// SPDX-License-Identifier: GPL-2.0-or-later
// Reuse the production-test uniform conversion, not a substitute GLSL compiler.
#define main inherited_favorites_test_main
#include "render_favorites.cpp"
#undef main
#include "BasicEffectState.h"

static Uniforms BasicUniforms(const json& spec,const json& overrides,float time)
{
    json parameters=json::object();for(const auto& c:spec.at("controls"))parameters[c.at("key").get<std::string>()]=c.at("default");
    parameters.update(overrides);
    auto uniforms=Parameters(spec,parameters,time);
    uniforms.custom["iTap"]={{0,0,0,0},4};uniforms.custom["iTapCount"].values[0]=0;
    native_basic::State state;
    auto native=state.Update(spec.at("id"),parameters,0);
    for(int i=0;i<int(time*60);++i)native=state.Update(spec.at("id"),parameters,1.0/60);
    for(const auto& pair:native)uniforms.custom[pair.first]=pair.second;
    return uniforms;
}
static std::string Prefix(const json& spec)
{
    std::string prefix="uniform vec4 iTap;\nuniform float iTapCount;\nuniform vec4 iTapEvents[64];\nuniform vec4 iTapMeta[64];\n";
    for(const auto& c:spec.at("controls"))
    {
        const std::string key=c.at("key"),type=c.at("type");
        prefix+="uniform "+std::string(type=="color"?"vec3":"float")+" p_"+key+";\n";
        if(type=="number")prefix+="uniform float t_"+key+";\n";
    }
    return prefix+native_basic::State::Declarations(spec.at("id"));
}
static int BasicRender(const QString& repo,const QString& output)
{
    try
    {
        QOffscreenSurface surface;surface.create();QOpenGLContext context;
        context.setFormat(surface.format());Check(context.create()&&context.makeCurrent(&surface),"GL context unavailable");
        QDir().mkpath(output);
        const auto catalogue=Read(repo+"/tests/signal-favorites/basic-family.metadata.json");
        const auto cases=Read(repo+"/tests/signal-favorites/basic-family.cases.json");
        json evidence=json::array();unsigned assertions=0;
        for(auto entry=catalogue.begin();entry!=catalogue.end();++entry)
        {
            const auto slug=QString::fromStdString(entry.key());
            const auto spec=Read(repo+"/Effects/SignalFavorites/presets/"+slug+".json");
            QFile shader(repo+"/shaders/SignalFavorites/"+slug+".fs");Check(shader.open(QIODevice::ReadOnly),"shader missing");
            ShaderProgram program;program.SetVersion("130");program.Resize(800,500);
            program.main_pass->data.feedback=spec.value("feedback",false);
            program.main_pass->data.fragment_shader=Prefix(spec)+shader.readAll().toStdString();
            program.Init();const auto log=program.Compile();if(!log.isEmpty())throw std::runtime_error(entry.key()+": "+log.toStdString());
            auto draw=[&](const Uniforms& uniforms){program.Draw(uniforms,context.functions());auto im=program.Image();Check(im.width()==800&&im.height()==500,"wrong GPU dimensions");Check(context.functions()->glGetError()==GL_NO_ERROR,"GL error");return im;};
            auto image=draw(BasicUniforms(spec,json::object(),1.25f));++assertions;
            image.save(output+"/"+slug+".png");
            unsigned variants=0;
            for(const auto& c:spec.at("controls"))
            {
                const std::string key=c.at("key"),type=c.at("type");std::vector<json> values;
                if(type=="number")values={c.at("min"),c.at("max")};
                else if(type=="boolean")values={false,true};
                else if(type=="enum")for(const auto& v:c.at("options"))values.push_back(v);
                else values={"#000000","#ffffff"};
                for(const auto& value:values){draw(BasicUniforms(spec,{{key,value}},1.25f));++variants;++assertions;}
            }
            unsigned samples=0;
            for(const auto& test:cases)
            {
                if(test.at("slug")!=entry.key())continue;
                auto uniforms=BasicUniforms(spec,test.value("parameters",json::object()),test.value("time",0.f));
                const auto explicit_uniforms=test.value("uniforms",json::object());
                for(auto v=explicit_uniforms.begin();v!=explicit_uniforms.end();++v)
                {
                    auto& target=uniforms.custom[v.key()];
                    if(v.value().is_array()){target.components=unsigned(v.value().size());for(unsigned c=0;c<target.components;++c)target.values[c]=v.value()[c];}
                    else target.values[0]=v.value().get<float>();
                }
                image=draw(uniforms);
                const int x=int(test.at("point")[0].get<double>()*image.width()/320),y=int(test.at("point")[1].get<double>()*image.height()/200);
                const auto color=image.pixelColor(x,y);const double rgb[]={color.redF(),color.greenF(),color.blueF()};
                for(unsigned c=0;c<3;++c)
                    if(std::abs(rgb[c]-test.at("rgb")[c].get<double>())>.015)
                        throw std::runtime_error(entry.key()+" checkpoint "+test.at("name").get<std::string>()+" channel"+std::to_string(c)+" actual="+std::to_string(rgb[c]));
                ++samples;++assertions;
            }
            if(entry.key()=="neon-shift")
            {
                // Recompile resets history; convergence must retain the actual
                // previous texture rather than sampling the draw target.
                program.Compile();auto u=BasicUniforms(spec,json::object(),0);
                const auto a=draw(u),b=draw(u);const auto ca=a.pixelColor(100,100),cb=b.pixelColor(100,100);
                Check(cb.red()>ca.red()&&cb.blue()>ca.blue(),"feedback did not accumulate");
                Check(cb.red()<=2*ca.red()+2,"feedback accumulation out of range");++assertions;
            }
            evidence.push_back({{"slug",entry.key()},{"variants",variants},{"analytic_points",samples},{"size",{800,500}}});
            program.CleanupGL();
        }
        std::ofstream(QDir(output).filePath("basic-family-gpu.json").toStdString())<<evidence.dump(2)<<'\n';
        std::cout<<"PASS 10 basic GPU ports / "<<assertions<<" checks: production shader, 800x500, controls, analytic colors, native histories/feedback\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;QGuiApplication app(argc,argv);int result=3;std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{result=BasicRender(argv[1],argv[2]);QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(60000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return result;
}
