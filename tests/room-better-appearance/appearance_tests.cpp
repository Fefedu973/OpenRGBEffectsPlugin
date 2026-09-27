// SPDX-License-Identifier: GPL-2.0-or-later
#include "Effects/BetterCapture/Appearance.h"
#include "Effects/Shaders/ShaderRenderGraph.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QFile>
#include <QDir>
#include <QVector4D>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <limits>
using json=nlohmann::json;
static unsigned assertions=0;
static void Check(bool okay,const char* text){++assertions;if(!okay)throw std::runtime_error(text);}
static QString Read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read "+path.toStdString());return QString::fromUtf8(f.readAll());}
class Renderer {
    ShaderRenderGraphRunner runner;
    ShaderRenderGraphFrame frame;
    const better_capture::Prepared* current=nullptr;
    std::uint64_t sequence=0;
public:
    explicit Renderer(QOpenGLFunctions*){frame.expires=std::chrono::steady_clock::time_point::max();}
    void Upload(const std::string& key,const QImage& input){auto image=std::make_shared<DynamicShaderImage>();image->sequence=++sequence;image->image=input;frame.images[key]=image;}
    QImage Draw(const QString& root,const better_capture::Prepared& prepared){
        if(current!=&prepared){
        const auto common=Read(root+"/"+QString::fromStdString(prepared.graph.preamble));
        if(prepared.geometryRGBA){auto geometry=std::make_shared<DynamicShaderImage>();geometry->sequence=1;geometry->width=prepared.geometryWidth;geometry->height=prepared.geometryHeight;geometry->rgba32f=prepared.geometryRGBA;frame.images["geometry"]=geometry;}
        auto graph=std::make_shared<ShaderRenderGraph>();graph->output=prepared.graph.output;
        for(const auto& p:prepared.graph.passes){
            ShaderRenderGraph::Pass pass;pass.id=p.id;pass.fragment=(common+Read(root+"/"+QString::fromStdString(p.shader))).toStdString();pass.inputs=p.inputs;pass.width=p.width;pass.height=p.height;
            for(const auto& item:p.uniforms)pass.uniforms[item.first]={item.second.value,int(item.second.components)};
            graph->passes.push_back(std::move(pass));
        }
        frame.graph=graph;current=&prepared;}
        const auto& last=prepared.graph.passes.back();auto image=runner.Draw(frame,last.width,last.height);
        Check(QOpenGLContext::currentContext()->functions()->glGetError()==GL_NO_ERROR,"Production graph GL error");return image;
    }
};
static QImage Flatten(const QImage& input){QImage out(input.size(),QImage::Format_ARGB32);for(int y=0;y<out.height();++y)for(int x=0;x<out.width();++x){auto c=input.pixelColor(x,y);const double a=c.alphaF();out.setPixel(x,y,qRgb(int(std::round(c.red()*a)),int(std::round(c.green()*a)),int(std::round(c.blue()*a))));}return out;}
static json Compare(const QImage& actual,const QImage& expected){Check(actual.size()==expected.size(),"Reference dimensions changed");double sum=0,square=0;unsigned max=0,above=0;std::vector<unsigned> values;values.reserve(actual.width()*actual.height()*3);for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width();++x){const auto a=actual.pixelColor(x,y),b=expected.pixelColor(x,y);for(const int d:{std::abs(a.red()-b.red()),std::abs(a.green()-b.green()),std::abs(a.blue()-b.blue())}){sum+=d;square+=d*d;max=std::max(max,unsigned(d));above+=d>8;values.push_back(unsigned(d));}}std::sort(values.begin(),values.end());return {{"mae",sum/values.size()},{"rms",std::sqrt(square/values.size())},{"p99",values[values.size()*99/100]},{"maximum",max},{"above8_fraction",double(above)/values.size()}};}
static double Milliseconds(std::chrono::steady_clock::time_point start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
static json Timing(std::vector<double> samples){std::sort(samples.begin(),samples.end());double sum=0;for(double v:samples)sum+=v;return {{"mean_ms",sum/samples.size()},{"median_ms",samples[samples.size()/2]},{"p95_ms",samples[std::min(samples.size()-1,samples.size()*95/100)]},{"maximum_ms",samples.back()}};}
static json Rectangle(double x,double y,double w,double h){
    json crop=json::array(),placed=json::array();for(const auto& p:std::vector<std::pair<double,double>>{{0,0},{w,0},{w,h},{0,h}}){crop.push_back({{"x",p.first},{"y",p.second}});placed.push_back({{"x",x+p.first},{"y",y+p.second}});}
    return {{"contributes",true},{"canvasWidth",w},{"canvasHeight",h},{"canvasFromImage",{{"m11",1},{"m12",0},{"m21",0},{"m22",1},{"dx",x},{"dy",y}}},{"cropPolygonLocal",crop},{"placedCoveragePolygon",placed}};
}
static void Rejected(const json& m,unsigned w=320,unsigned h=200){bool rejected=false;try{better_capture::Prepare(m,w,h);}catch(const std::exception&){rejected=true;}Check(rejected,"Invalid metadata/budget accepted");}
static void GeometryChecks(json m){
    auto& s=m["effectiveSettings"];s["screenX"]=0;s["screenY"]=0;s["screenWidth"]=320;s["screenHeight"]=200;s["ambilight"]=true;s["ambilightStyle"]="Contours";s["ambilightEdgeDepth"]=20;s["ambilightEdgeMix"]=0;
    m["sources"]=json::array({Rectangle(20,40,10,120),Rectangle(33,40,100,120)});auto prepared=better_capture::Prepare(m,320,200);
    Check(qAlpha(prepared.silhouette.pixel(25,100))==255,"Black pixels must not create geometry holes");Check(qAlpha(prepared.silhouette.pixel(31,100))==0,"Separate sources must preserve gaps");
    const auto at=(320*200+100*320)*4+1;const float limit=prepared.geometryRGBA->at(at)*255;
    Check(limit>1&&limit<70,"Contour color band crossed an unrelated source across a gap");
    m["sources"]=json::array({Rectangle(20,40,80,120),Rectangle(60,40,80,120)});prepared=better_capture::Prepare(m,320,200);
    Check(qAlpha(prepared.silhouette.pixel(70,100))==255,"Overlapping sources produced an internal hole");
    const auto seed=(100*320+65)*4;const double sx=prepared.geometryRGBA->at(seed)*320,sy=prepared.geometryRGBA->at(seed+1)*200;
    Check(sx<21||sx>138||sy<41||sy>158,"Overlapping sources produced an internal boundary");
    m["sources"]=json::array();prepared=better_capture::Prepare(m,320,200);Check(prepared.geometryRGBA->at(seed+3)==0,"Empty geometry produced a boundary");
    m["sources"]=json::array({Rectangle(20,40,10,120)});m["sources"][0]["canvasFromImage"]["dx"]=std::numeric_limits<double>::infinity();Rejected(m);
    m["sources"]=json::array({Rectangle(20,40,10,120)});m["sources"][0]["placedCoveragePolygon"][0]["x"]=-1;Rejected(m);
    m["sources"]=std::vector<json>(129,Rectangle(20,40,10,120));Rejected(m);
    m["sources"]=json::array();Rejected(m,0,200);Rejected(m,4096,4096);
    s["ambilight"]=false;s["blur"]=true;Rejected(m,2048,2048);
    m["canvasWidth"]=0;Rejected(m);
}
int main(int argc,char**argv){if(argc!=4)return 2;QGuiApplication app(argc,argv);try{
    const QString root=argv[1],fixtures=argv[2],output=argv[3];QDir().mkpath(output);
    QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());Check(context.create()&&context.makeCurrent(&surface),"GL context unavailable");
    const auto cases=json::parse(Read(fixtures+"/cases.json").toStdString());json report=json::array();
    for(const auto& fixture:cases.at("cases")){
        const auto name=fixture.at("name").get<std::string>();const auto& metadata=fixture.at("metadata");
        const unsigned w=metadata.at("outputWidth"),h=metadata.at("outputHeight");auto prepared=better_capture::Prepare(metadata,w,h);
        Check(prepared.graph.passes.size()<=32,"Unbounded graph");std::set<std::string> ids={"raw","coverage","geometry"};std::uint64_t bytes=0;
        for(const auto&p:prepared.graph.passes){for(const auto&i:p.inputs)Check(i.empty()||ids.count(i),"Invalid graph order");Check(ids.insert(p.id).second,"Duplicate pass ID");bytes+=std::uint64_t(p.width)*p.height*8;}
        Check(bytes<=128ULL*1024*1024,"Graph exceeds renderer budget");
        QImage scene(fixtures+"/"+QString::fromStdString(name)+"-scene.png"),coverage(fixtures+"/"+QString::fromStdString(name)+"-coverage.png");Check(!scene.isNull()&&!coverage.isNull(),"Reference source missing");
        Renderer renderer(context.functions());renderer.Upload("raw",Flatten(scene));renderer.Upload("coverage",coverage);const auto image=renderer.Draw(root,prepared);image.save(output+"/"+QString::fromStdString(name)+".png");
        const QImage reference(fixtures+"/"+QString::fromStdString(name)+"-appearance.png");Check(!reference.isNull(),"Reference appearance missing");auto metrics=Compare(image,reference);metrics["name"]=name;metrics["passes"]=prepared.graph.passes.size();metrics["intermediate_bytes"]=bytes;
        const QImage gpuReference(output+"/"+QString::fromStdString(name)+"-webgl.png");if(!gpuReference.isNull())metrics["webgl_reference"]=Compare(image,gpuReference);
        if(!prepared.silhouette.isNull()){prepared.silhouette.save(output+"/"+QString::fromStdString(name)+"-native-mask.png");const QImage expectedMask(output+"/"+QString::fromStdString(name)+"-placed-mask.png");if(!expectedMask.isNull()){unsigned mismatch=0;double error=0;Check(prepared.silhouette.size()==expectedMask.size(),"Mask sizes differ");for(int y=0;y<expectedMask.height();++y)for(int x=0;x<expectedMask.width();++x){const int a=qAlpha(prepared.silhouette.pixel(x,y)),b=qAlpha(expectedMask.pixel(x,y));mismatch+=(a>=128)!=(b>=128);error+=std::abs(a-b);}metrics["mask_occupancy_mismatches"]=mismatch;metrics["mask_mae"]=error/(expectedMask.width()*expectedMask.height());}}
        const auto& settings=metadata.at("effectiveSettings");const bool halo=settings.value("ambilight",true);const auto style=settings.value("ambilightStyle",std::string("Classic"));
        if(!halo){Check(metrics["mae"].get<double>()<.3&&metrics["maximum"].get<int>()<=8,"Picture/filter optical regression");}
        else if(style=="Contours"){Check(metrics["mae"].get<double>()<1.25&&metrics["p99"].get<int>()<=20&&metrics["above8_fraction"].get<double>()<.035,"Contour optical regression");}
        else {Check(metrics["mae"].get<double>()<1&&metrics["p99"].get<int>()<=4&&metrics["maximum"].get<int>()<=16,"Classic/Soft optical regression");}
        report.push_back(metrics);std::cout<<name<<" "<<metrics.dump()<<"\n";
    }
    GeometryChecks(cases["cases"][0]["metadata"]);
    json benchmarks=json::array();
    for(const std::string name:{"classic-inset","soft-fullscreen","contours-crop-hq"}){
        auto item=std::find_if(cases["cases"].begin(),cases["cases"].end(),[&](const json& f){return f["name"]==name;});Check(item!=cases["cases"].end(),"Benchmark fixture missing");
        const auto m=item->at("metadata");std::vector<double> prepareTimes;for(unsigned n=0;n<6;++n){const auto begin=std::chrono::steady_clock::now();const auto p=better_capture::Prepare(m,800,600);prepareTimes.push_back(Milliseconds(begin));}
        auto prepared=better_capture::Prepare(m,800,600);QImage raw=Flatten(QImage(fixtures+"/"+QString::fromStdString(name)+"-scene.png")).scaled(800,600),coverage=QImage(fixtures+"/"+QString::fromStdString(name)+"-coverage.png").scaled(800,600);
        Renderer renderer(context.functions());renderer.Upload("raw",raw);renderer.Upload("coverage",coverage);for(unsigned n=0;n<3;++n)renderer.Draw(root,prepared);
        std::vector<double> frames;for(unsigned n=0;n<24;++n){const auto begin=std::chrono::steady_clock::now();renderer.Upload("raw",raw);renderer.Upload("coverage",coverage);auto image=renderer.Draw(root,prepared);Check(!image.isNull()&&image.size()==QSize(800,600),"Benchmark graph missing image");frames.push_back(Milliseconds(begin));}
        benchmarks.push_back({{"name",name},{"width",800},{"height",600},{"prepare",Timing(prepareTimes)},{"upload_render_readback",Timing(frames)}});std::cout<<"benchmark "<<benchmarks.back().dump()<<"\n";
    }
    // Real GPU execution at all appearance control bounds. Geometry is empty so
    // this checks bounded allocation/shader behavior, not another optical oracle.
    for(const std::string style:{"Classic","Soft","Contours"})for(int maximum:{0,1}){
        auto m=cases["cases"][0]["metadata"];auto& s=m["effectiveSettings"];s["ambilight"]=true;s["ambilightStyle"]=style;s["ambilightBlur"]=maximum?100:0;s["ambilightSpread"]=maximum?100:0;s["ambilightSaturation"]=maximum?10:0;s["ambilightIntensity"]=maximum?200:0;s["ambilightCutoff"]=maximum?100:0;s["ambilightFullscreen"]=true;s["hideSources"]=true;s["hue"]=maximum?180:-180;s["brightness"]=maximum?100:-100;s["saturation"]=maximum?100:-100;s["blur"]=true;s["ambilightEdgeDepth"]=maximum?20:1;s["ambilightEdgeMix"]=maximum?30:0;s["ambilightEdgeReach"]=maximum?200:1;s["ambilightEdgeFade"]=maximum?100:0;s["interpolation"]="pixelated";
        m["sources"]=json::array();auto prepared=better_capture::Prepare(m,800,600);Renderer renderer(context.functions());QImage raw(320,200,QImage::Format_RGB32);raw.fill(Qt::black);renderer.Upload("raw",raw);renderer.Upload("coverage",raw);const auto result=renderer.Draw(root,prepared);Check(!result.isNull(),"Extreme graph failed");Check(result.pixelColor(400,300)==QColor(Qt::black),"Absent source created colored light");
    }
    std::ofstream((output+"/appearance-comparison.json").toStdString())<<json{{"assertions",assertions},{"cases",report},{"benchmarks",benchmarks},{"renderer",reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER))},{"comparison","Synthetic production web fixtures; premultiplied raw reconstructed from scene PNG; no capture or live API"}}.dump(2)<<'\n';std::cout<<"PASS "<<assertions<<" structural/GPU/optical assertions; image metrics and benchmarks reported\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
