// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic pixels only. Never creates a screen capturer or reads a desktop.
#include "Effects/SignalFavorites/DominantScreenColor.h"
#include <QColor>
#include <iostream>
#include <chrono>
#include <stdexcept>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(#x);}while(0)
static bool Near(float a,float b){return std::abs(a-b)<.005f;}
int main()
{
    try
    {
        using namespace native_screen_color;
        CHECK(!Dominant({}).valid);
        QImage image(100,50,QImage::Format_ARGB32);image.fill(Qt::transparent);
        CHECK(!Dominant(image).valid);
        image.fill(Qt::black);auto r=Dominant(image);
        CHECK(!r.valid); // caller uses explicit Static Color 1, never held history
        for(int gray:{24,60,127,255}){image.fill(QColor(gray,gray,gray));CHECK(!Dominant(image).valid);}
        image.fill(Qt::red);
        for(int x=60;x<100;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,Qt::blue);
        r=Dominant(image);
        CHECK(r.valid&&Near(r.fraction,.6f));CHECK(r.rgb[0]==1&&r.rgb[1]==0&&r.rgb[2]==0);
        // Dark and light shades combine by hue; their value never modulates
        // audio brightness a second time.
        for(int x=0;x<60;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,QColor(x<30?240:254,0,0));
        r=Dominant(image);CHECK(r.rgb[0]==1&&r.rgb[2]==0);
        // Alpha weighs actual coverage: a transparent red overlay cannot win.
        for(int x=0;x<60;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,QColor(255,0,0,20));
        r=Dominant(image);CHECK(r.rgb[0]==0&&r.rgb[2]==1);
        // 80% padding cannot defeat the useful colored part of the capture.
        image.fill(Qt::black);
        for(int x=80;x<100;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,x<92?Qt::red:Qt::blue);
        r=Dominant(image);CHECK(r.valid&&Near(r.fraction,.12f));CHECK(r.rgb[0]==1&&r.rgb[2]==0);
        // A small blue photo on a neutral app is useful; isolated color noise is not.
        image.fill(QColor(45,45,45));
        for(int x=95;x<100;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,Qt::blue);
        r=Dominant(image);CHECK(r.valid&&r.rgb[2]==1&&Near(r.fraction,.05f));
        image.fill(Qt::black);for(int x=0;x<5;++x)image.setPixelColor(x,0,Qt::red);
        CHECK(!Dominant(image).valid); // 0.1% / five samples
        for(int x=0;x<30;++x)image.setPixelColor(x,0,Qt::red);
        CHECK(Dominant(image).valid); // 0.6% / thirty samples
        // Circular grouping combines red across 359/0 degrees, without gray.
        for(int x=0;x<100;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,QColor::fromHsvF((x<50?359.f:1.f)/360.f,1,1));
        r=Dominant(image);CHECK(r.valid&&r.rgb[0]>.99f&&r.rgb[1]<.025f&&r.rgb[2]<.025f);
        for(float value:{.12f,.9f})
        {image.fill(QColor::fromHsvF(0,1,value));r=Dominant(image);CHECK(r.valid&&r.rgb[0]==1&&r.rgb[1]==0&&r.rgb[2]==0);}
        image.fill(QColor::fromHsvF(.61f,.55f,.3f));r=Dominant(image);
        QColor selected=QColor::fromRgbF(r.rgb[0],r.rgb[1],r.rgb[2]);
        // Compare the actual RGB8 source, not the pre-quantization QColor.
        CHECK(r.valid&&Near(selected.hsvHueF(),image.pixelColor(0,0).hsvHueF()));
        CHECK(Near(selected.hsvSaturationF(),image.pixelColor(0,0).hsvSaturationF())&&selected.valueF()==1);
        for(const QColor& rejected:{QColor::fromHsvF(0,1,.07f),QColor::fromHsvF(0,.2f,1),QColor::fromHsvF(0,.3f,.1f)})
        {image.fill(rejected);CHECK(!Dominant(image).valid);}
        image.fill(Qt::gray);
        for(int x=0;x<75;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,x<40?Qt::green:Qt::blue);
        r=Dominant(image);CHECK(r.valid&&r.rgb[1]==1&&Near(r.fraction,.4f));
        image.fill(Qt::green);State state;
        r=state.Update(image,10);CHECK(r.rgb[1]==1);
        image.fill(Qt::blue);r=state.Update(image,10.05);CHECK(r.rgb[1]==1); // bounded 10Hz
        r=state.Update(image,10.101);CHECK(r.rgb[2]==1);
        CHECK(!state.Update({},10.11).valid); // absence never retains old color
        image.fill(Qt::red);r=state.Update(image,10.12);CHECK(r.rgb[0]==1);
        image.fill(Qt::blue);r=state.Update(image,2);CHECK(r.rgb[2]==1); // new clock
        state.Reset();r=state.Update(image,2.01);CHECK(r.rgb[2]==1);
        image.fill(Qt::gray);CHECK(!state.Update(image,2.111).valid);
        CHECK(!state.Update(image,100).valid); // no indefinite old blue on a static gray frame
        state.Reset();image.fill(Qt::transparent);CHECK(!state.Update(image,3).valid);
        image.fill(Qt::red);CHECK(!state.Update(image,3.05).valid);
        CHECK(state.Update(image,3.101).valid);
        QImage huge(4096,2160,QImage::Format_RGB32);huge.fill(Qt::cyan);
        r=Dominant(huge);CHECK(r.samples==128*72);CHECK(r.rgb[1]==1&&r.rgb[2]==1);
        QImage tiny(1,1,QImage::Format_RGB32);tiny.fill(Qt::yellow);CHECK(Dominant(tiny).valid);
        // Ties are deterministic, not randomized between frames.
        QImage tie(2,1,QImage::Format_RGB32);tie.setPixelColor(0,0,Qt::red);tie.setPixelColor(1,0,Qt::blue);
        CHECK(Dominant(tie).rgb==Dominant(tie).rgb);
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned i=0;i<100;++i)CHECK(Dominant(huge).valid);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/100;
        std::cout<<"PASS "<<checks<<" screen palette checks (synthetic only)\n";
        std::cout<<"Full bounded 128x72 palette reduction: "<<ms<<" ms mean (4096x2160 synthetic source, 100 iterations)\n";
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
