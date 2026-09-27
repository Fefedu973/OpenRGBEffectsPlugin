// SPDX-License-Identifier: GPL-2.0-or-later
#define main screen_inherited_test_main
#include "render_favorites.cpp"
#undef main
#include "ScreenEffectState.h"
static std::string ScreenPrefix(const json&spec){std::string prefix=native_screen::State::Declarations(spec.at("id"));for(const auto&c:spec.at("controls")){const std::string type=c.at("type"),key=c.at("key");prefix+="uniform "+std::string(type=="color"?"vec3":"float")+" p_"+key+";\n";if(type=="number")prefix+="uniform float t_"+key+";\n";}return prefix;}
static std::string ShaderFile(const QString&path){QFile f(path);Check(f.open(QIODevice::ReadOnly),"screen shader missing");return f.readAll().toStdString();}
static unsigned assertions=0;
static void Pixel(const QImage&i,int x,int y,std::array<double,3> expected,double tolerance=.012){const auto color=i.pixelColor(x,y);const double actual[]={color.redF(),color.greenF(),color.blueF()};for(unsigned c=0;c<3;++c)if(std::abs(actual[c]-expected[c])>tolerance)throw std::runtime_error("screen RGB channel "+std::to_string(c)+" actual="+std::to_string(actual[c])+" expected="+std::to_string(expected[c]));++assertions;}
static int Run(const QString&root,const QString&output){try{
 QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());Check(context.create()&&context.makeCurrent(&surface),"screen GL context");auto*gl=context.functions();QDir().mkpath(output);json evidence=json::array();
 for(const QString slug:{"average-color","screen-ambience","lsd-ambience"}){
  const json spec=Read(root+"/Effects/SignalFavorites/presets/"+slug+".json");ShaderProgram program;program.SetVersion("130");program.Resize(800,500);
  for(unsigned slot=0;slot<2;++slot){auto*p=new ShaderPass(ShaderPass::DYNAMIC_IMAGE);p->data.image_slot=slot;program.passes.push_back(p);}
  for(const auto&p:spec.value("passes",json::array())){auto*pass=new ShaderPass(ShaderPass::BUFFER);pass->data.fragment_shader=ScreenPrefix(spec)+ShaderFile(root+"/shaders/SignalFavorites/"+QString::fromStdString(p.at("shader")));program.passes.push_back(pass);}
  program.main_pass->data.fragment_shader=ScreenPrefix(spec)+ShaderFile(root+"/shaders/SignalFavorites/"+slug+".fs");program.Init();const auto errors=program.Compile();if(!errors.isEmpty())throw std::runtime_error(errors.toStdString());
  auto state=std::make_unique<native_screen::State>();QImage source(28,20,QImage::Format_RGB32);source.fill(Qt::red);uint64_t sequence=1;
  auto draw=[&](json edits=json::object(),unsigned taps=0,bool available=true){json params=json::object();for(const auto&c:spec.at("controls"))params[c.at("key").get<std::string>()]=c.at("default");if(edits.is_object())params.update(edits);auto u=Parameters(spec,params,0);auto result=state->Update(spec.at("id"),params,.033,source,sequence,taps);for(const auto&kv:result.uniforms)u.custom[kv.first]=kv.second;
   auto image=std::make_shared<DynamicShaderImage>();image->image=source;image->sequence=sequence;auto numeric=std::make_shared<DynamicShaderImage>();numeric->width=result.width;numeric->height=result.height;numeric->sequence=result.sequence;numeric->rgba32f=result.numericRGBA;u.images[0]=available?image:nullptr;u.images[1]=numeric;program.Draw(u,gl);Check(gl->glGetError()==GL_NO_ERROR,"screen GL error");auto rendered=program.Image();Check(rendered.size()==QSize(800,500),"screen output size");return rendered;};
  draw().save(output+"/"+slug+".png");unsigned variants=0;
  for(const auto&c:spec.at("controls")){const std::string type=c.at("type"),key=c.at("key");std::vector<json> values;if(type=="number")values={c.at("min"),c.at("max")};else if(type=="boolean")values={false,true};else if(type=="enum")for(const auto&v:c.at("options"))values.push_back(v);else values={"#000000","#ffffff"};for(const auto&v:values){state->Reset();draw({{key,v}});++assertions;++variants;}}
  state->Reset();Pixel(draw({},0,false),400,250,{0,0,0});
  if(slug=="average-color"){
   state->Reset();Pixel(draw(),400,250,{1,0,0});state->Reset();Pixel(draw({{"tapOn",true}},2),400,250,{.9025,0,0});
   for(int y=10;y<20;++y)for(int x=0;x<28;++x)source.setPixelColor(x,y,Qt::blue);++sequence;state->Reset();Pixel(draw(),400,250,{0,1,0});
  }else if(slug=="screen-ambience"){
   for(const auto&mode:std::vector<std::pair<std::string,std::array<double,3>>>{{"Standard",{1,0,0}},{"Cinema",{.95,.05,.05}},{"Mono",{.5,.5,.5}},{"Vivid",{1,0,0}},{"Dominant",{1,0,0}},{"HD",{1,0,0}}}){state->Reset();Pixel(draw({{"picture_mode",mode.first},{"blur_amount",0}}),400,250,mode.second);}
   for(const auto&setting:std::vector<std::pair<std::string,std::array<double,3>>>{{"brightness",{0,0,0}},{"saturation",{.213,.213,.213}},{"contrast",{.5,.5,.5}}}){state->Reset();Pixel(draw({{setting.first,-100},{"blur_amount",0}}),400,250,setting.second);}
   state->Reset();Pixel(draw({{"boost",180},{"blur_amount",0}}),400,250,{0,.426,.426});
   for(int y=0;y<20;++y)for(int x=0;x<28;++x)source.setPixelColor(x,y,y<10?(x<14?Qt::red:Qt::green):(x<14?Qt::blue:Qt::white));++sequence;
   for(const std::string mode:{"Standard","HD"}){state->Reset();auto out=draw({{"picture_mode",mode},{"blur_amount",0}});Pixel(out,100,100,{1,0,0});Pixel(out,700,100,{0,1,0});Pixel(out,100,400,{0,0,1});Pixel(out,700,400,{1,1,1});}
   source.fill(Qt::white);++sequence;state->Reset();auto blur=draw({{"blur_amount",20}});Pixel(blur,400,250,{1,1,1});const auto edge=blur.pixelColor(0,0);Check(edge.redF()>.20&&edge.redF()<.32,"transparent blur edge");++assertions;
  }else{
   source.fill(Qt::blue);++sequence;state->Reset();auto dots=draw({{"colorMode","Screen"}});Pixel(dots,12,12,{0,0,1});Pixel(dots,26,25,{0,0,0});
   state->Reset();Pixel(draw({{"colorMode","Custom"},{"color1","#ff0000"}}),12,12,{1,0,0});
  }
  std::vector<double> ms;for(unsigned i=0;i<12;++i){QElapsedTimer timer;timer.start();draw();ms.push_back(timer.nsecsElapsed()/1e6);}std::sort(ms.begin(),ms.end());
  json worst=json::object();if(slug=="screen-ambience"){
   std::vector<double> times;for(unsigned i=0;i<12;++i){QElapsedTimer timer;timer.start();draw({{"blur_amount",20}});times.push_back(timer.nsecsElapsed()/1e6);}std::sort(times.begin(),times.end());worst={{"blur_amount",20},{"median_ms",times[6]},{"maximum_ms",times.back()}};
  }
  evidence.push_back({{"id",spec.at("id")},{"variants",variants},{"median_ms",ms[6]},{"maximum_ms",ms.back()},{"maximum_control_case",worst},{"dimensions",{800,500}},{"measurement","State update plus upload, real production shader passes and synchronous readback; synthetic input only"}});
  program.CleanupGL();
 }
 std::ofstream((output+"/screen-gpu.json").toStdString())<<json{{"assertions",assertions},{"effects",evidence}}.dump(2)<<'\n';std::cout<<"PASS "<<assertions<<" native screen GPU checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
int main(int argc,char**argv){if(argc!=3)return 2;QGuiApplication app(argc,argv);int code=3;std::thread worker;QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{code=Run(argv[1],argv[2]);QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});QTimer::singleShot(120000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return code;}
