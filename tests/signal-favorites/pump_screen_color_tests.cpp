// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic pixels only. Never creates a screen capturer or reads a desktop.
#include "Effects/SignalFavorites/DominantScreenColor.h"
#include <QColor>
#include <iostream>
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
        CHECK(r.valid&&r.rgb[0]==0&&r.rgb[1]==0&&r.rgb[2]==0&&r.fraction==1);
        image.fill(Qt::red);
        for(int x=60;x<100;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,Qt::blue);
        r=Dominant(image);
        CHECK(r.valid&&Near(r.fraction,.6f));CHECK(r.rgb[0]==1&&r.rgb[1]==0&&r.rgb[2]==0);
        // Adjacent shades in the same RGB4 bucket combine into a population;
        // other buckets never influence the selected color's centroid.
        for(int x=0;x<60;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,QColor(x<30?240:254,0,0));
        r=Dominant(image);CHECK(Near(r.rgb[0],247.f/255)&&r.rgb[2]==0);
        // Alpha weighs actual coverage: a transparent red overlay cannot win.
        for(int x=0;x<60;++x)for(int y=0;y<50;++y)image.setPixelColor(x,y,QColor(255,0,0,20));
        r=Dominant(image);CHECK(r.rgb[0]==0&&r.rgb[2]==1);
        image.fill(Qt::green);State state;
        r=state.Update(image,10);CHECK(r.rgb[1]==1);
        image.fill(Qt::blue);r=state.Update(image,10.05);CHECK(r.rgb[1]==1); // bounded 10Hz
        r=state.Update(image,10.101);CHECK(r.rgb[2]==1);
        CHECK(!state.Update({},10.11).valid); // absence never retains old color
        image.fill(Qt::red);r=state.Update(image,10.12);CHECK(r.rgb[0]==1);
        image.fill(Qt::blue);r=state.Update(image,2);CHECK(r.rgb[2]==1); // new clock
        state.Reset();r=state.Update(image,2.01);CHECK(r.rgb[2]==1);
        state.Reset();image.fill(Qt::transparent);CHECK(!state.Update(image,3).valid);
        image.fill(Qt::red);CHECK(!state.Update(image,3.05).valid);
        CHECK(state.Update(image,3.101).valid);
        QImage huge(4096,2160,QImage::Format_RGB32);huge.fill(Qt::cyan);
        r=Dominant(huge);CHECK(r.samples==128*72);CHECK(r.rgb[1]==1&&r.rgb[2]==1);
        // Ties are deterministic, not randomized between frames.
        QImage tie(2,1,QImage::Format_RGB32);tie.setPixelColor(0,0,Qt::red);tie.setPixelColor(1,0,Qt::blue);
        CHECK(Dominant(tie).rgb==Dominant(tie).rgb);
        std::cout<<"PASS "<<checks<<" screen palette checks (synthetic only)\n";
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
