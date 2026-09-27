// SPDX-License-Identifier: GPL-2.0-or-later
#include "ShaderRenderGraph.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static bool Near(QColor a,QColor b){return std::abs(a.red()-b.red())<=2&&std::abs(a.green()-b.green())<=2&&std::abs(a.blue()-b.blue())<=2;}
int main(int argc,char** argv)
{
    QGuiApplication app(argc,argv);
    try
    {
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"GL unavailable");
        ShaderRenderGraphRunner renderer;
        auto raw=std::make_shared<DynamicShaderImage>();raw->sequence=1;raw->generation=5;raw->image=QImage(2,2,QImage::Format_ARGB32);
        raw->image.setPixelColor(0,0,Qt::red);raw->image.setPixelColor(1,0,Qt::green);raw->image.setPixelColor(0,1,Qt::blue);raw->image.setPixelColor(1,1,Qt::white);
        auto graph=std::make_shared<ShaderRenderGraph>();
        for(unsigned i=0;i<12;++i)
        {
            ShaderRenderGraph::Pass pass;pass.id="p"+std::to_string(i);pass.width=pass.height=2;
            pass.inputs[0]=i?"p"+std::to_string(i-1):"raw";
            pass.fragment=i?"void mainImage(out vec4 c,in vec2 p){c=texture2D(iChannel0,p/iResolution.xy);}":
                "void mainImage(out vec4 c,in vec2 p){c=texture2D(iChannel0,vec2(p.x/iResolution.x,1.-p.y/iResolution.y));}";
            graph->passes.push_back(pass);
        }
        graph->output="p11";
        ShaderRenderGraphFrame frame;frame.graph=graph;frame.images["raw"]=raw;frame.expires=std::chrono::steady_clock::time_point::max();
        auto image=renderer.Draw(frame,2,2);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)Check(Near(image.pixelColor(x,y),raw->image.pixelColor(x,y)),"12-pass graph orientation/texture routing");
        auto changed=std::make_shared<DynamicShaderImage>(*raw);changed->sequence=2;changed->image.fill(Qt::yellow);frame.images["raw"]=changed;
        Check(Near(renderer.Draw(frame,2,2).pixelColor(0,0),Qt::yellow),"new source not uploaded");
        auto alpha=std::make_shared<ShaderRenderGraph>(*graph);alpha->passes[0].fragment="void mainImage(out vec4 c,in vec2 p){c=vec4(0.25,0,0,0.25);}";
        alpha->passes.back().fragment="void mainImage(out vec4 c,in vec2 p){vec4 s=texture2D(iChannel0,p/iResolution.xy);c=vec4(s.r,0,s.a,1);}";
        frame.graph=alpha;Check(Near(renderer.Draw(frame,2,2).pixelColor(0,0),QColor(64,0,64)),"intermediate alpha lost");
        frame.expires=std::chrono::steady_clock::now()-std::chrono::seconds(1);
        Check(Near(renderer.Draw(frame,2,2).pixelColor(0,0),Qt::black),"expired graph rendered");
        frame.expires=std::chrono::steady_clock::time_point::max();frame.images.clear();
        Check(Near(renderer.Draw(frame,2,2).pixelColor(0,0),Qt::black),"missing required source rendered");
        frame.images["raw"]=raw;
        auto cycle=std::make_shared<ShaderRenderGraph>(*graph);cycle->passes[0].inputs[0]="p11";frame.graph=cycle;
        bool rejected=false;try{renderer.Draw(frame,2,2);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"graph cycle accepted");
        auto huge=std::make_shared<ShaderRenderGraph>(*graph);for(auto& pass:huge->passes){pass.width=4096;pass.height=2048;}frame.graph=huge;
        rejected=false;try{renderer.Draw(frame,2,2);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"graph memory bound ignored");
        frame.graph=graph;Check(Near(renderer.Draw(frame,2,2).pixelColor(0,1),Qt::blue),"recovery after rejected graph");
        std::cout<<checks<<" GPU graph assertions PASS\n";return 0;
    }
    catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}
