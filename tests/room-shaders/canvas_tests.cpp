// SPDX-License-Identifier: GPL-2.0-or-later
#include "ShaderCanvas.h"
#include <cassert>
#include <chrono>
#include <iostream>
int main()
{
    assert(ShaderCanvas::ValidSize(800,600));
    assert(ShaderCanvas::ValidSize(3840,2160));
    assert(!ShaderCanvas::ValidSize(0,600));
    assert(!ShaderCanvas::ValidSize(4096,4096));
    assert(!ShaderCanvas::ValidSize(0xFFFFFFFFu,2));
    QImage frame(800,600,QImage::Format_ARGB32);
    for(int y=0;y<600;++y) for(int x=0;x<800;++x) frame.setPixel(x,y,qRgb(x%256,y%256,(x+y)%256));
    assert(ShaderCanvas::Sample(frame,0,0,800,600)==frame.pixel(0,0));
    assert(ShaderCanvas::Sample(frame,799,599,800,600)==frame.pixel(799,599));
    assert(ShaderCanvas::Sample(frame,0,0,1,1)==frame.pixel(400,300));
    const auto original=frame.pixel(250,200);
    const QImage black=ShaderCanvas::AdjustedBGRA(frame,0,0,0);
    assert(frame.pixel(250,200)==original);
    for(int y=0;y<600;++y) for(int x=0;x<800;++x) assert(black.pixel(x,y)==qRgb(0,0,0));
    auto bright=ShaderCanvas::AdjustedBGRA(frame,1,0,0);
    assert(bright==frame);
    auto warm=ShaderCanvas::AdjustedBGRA(frame,.5f,100,-50);
    const int expectedR=int(std::clamp<int>(250*(1+100/255.0),0,255)*.5f);
    const int expectedG=int(std::clamp<int>(200*(1-50/255.0),0,255)*.5f);
    assert(qRed(warm.pixel(250,200))==expectedR && qGreen(warm.pixel(250,200))==expectedG);
    const auto begin=std::chrono::steady_clock::now();
    for(int i=0;i<60;++i) bright=ShaderCanvas::AdjustedBGRA(frame,.7f,20,5);
    const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    std::cout << "Shader canvas validation PASS; 60 synthetic 800x600 CPU adjustment frames: " << elapsed << " ms total (not GPU/render FPS)\n";
}
