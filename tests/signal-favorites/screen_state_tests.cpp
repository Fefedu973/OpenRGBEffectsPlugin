// SPDX-License-Identifier: GPL-2.0-or-later
#include "ScreenEffectState.h"
#include <iostream>
#include <stdexcept>
#include <limits>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(#x);}while(0)
static bool near(double a,double b,double e=1e-6){return std::abs(a-b)<e;}
int main(){try{
 auto state=std::make_unique<native_screen::State>();QImage red(28,20,QImage::Format_RGB32);red.fill(Qt::red);QImage blue=red;blue.fill(Qt::blue);
 auto a=state->Update("AverageColor",{},.01,red,1,0);CHECK(a.width==1&&a.height==1);CHECK(near((*a.numericRGBA)[0],0)&&near((*a.numericRGBA)[1],1)&&near((*a.numericRGBA)[2],.5)&&near((*a.numericRGBA)[3],1));
 auto b=state->Update("AverageColor",{{"tapOn",true}},.01,red,1,2);CHECK(near((*b.numericRGBA)[3],.95*.95));auto c=state->Update("AverageColor",{{"tapOn",true}},.01,red,1,0);CHECK(near((*c.numericRGBA)[3],.95*.95+.025));CHECK(state->ReductionCount()==1);
 QImage split=red;for(int y=10;y<20;++y)for(int x=0;x<28;++x)split.setPixelColor(x,y,Qt::blue);a=state->Update("AverageColor",{},0,split,2,0);CHECK(near((*a.numericRGBA)[0],1.0/3)); // HSL hue120, not purple RGB averaging.
 state->Reset();a=state->Update("ScreenAmbience",{},.1,red,1,0);CHECK(a.width==28&&a.height==20);CHECK(near((*a.numericRGBA)[0],0));
 b=state->Update("ScreenAmbience",{},.1,blue,2,0);CHECK(near((*b.numericRGBA)[0],300.0/360));
 c=state->Update("ScreenAmbience",{},.1,blue,2,0);CHECK(near((*c.numericRGBA)[0],270.0/360));CHECK(state->ReductionCount()==2);
 auto hd=state->Update("ScreenAmbience",{{"picture_mode","HD"}},.1,red,3,0);CHECK(hd.width==160&&hd.height==100);CHECK(near((*hd.numericRGBA)[0],1));
 hd=state->Update("ScreenAmbience",{{"picture_mode","HD"}},.1,blue,4,0);CHECK(near((*hd.numericRGBA)[0],128.0/255)&&near((*hd.numericRGBA)[2],128.0/255));
 hd=state->Update("ScreenAmbience",{{"picture_mode","HD"}},.1,blue,4,0);CHECK(near((*hd.numericRGBA)[0],64.0/255)&&near((*hd.numericRGBA)[2],191.0/255));
 c=state->Update("ScreenAmbience",{},.1,blue,4,0);CHECK(near((*c.numericRGBA)[0],255.0/360)); // Previous standard history survived HD.
 state->Reset();QImage hue=red;hue.fill(QColor::fromHsl(350,255,128));a=state->Update("ScreenAmbience",{{"picture_mode","Dominant"}},.1,hue,1,0);CHECK(near(a.uniforms.at("bScreenDominantHue").values[0],350.0/360));
 hue.fill(QColor::fromHsl(10,255,128));b=state->Update("ScreenAmbience",{{"picture_mode","Dominant"}},.1,hue,2,0);CHECK(near(b.uniforms.at("bScreenDominantHue").values[0],349.0/360));
 state->Reset();hue.fill(Qt::red);hue.setPixelColor(27,19,QColor::fromHsl(120,64,180));a=state->Update("ScreenAmbience",{{"picture_mode","Dominant"}},.1,hue,1,0);CHECK(near(a.uniforms.at("bScreenDominantHue").values[0],120.0/360));
 state->Reset();a=state->Update("LSDAmbience",{},.01,blue,1,0);CHECK(a.numericRGBA->size()==560*4);CHECK(near((*a.numericRGBA)[0],242.0/360)&&near((*a.numericRGBA)[3],320.0/56));
 b=state->Update("LSDAmbience",{},.01,blue,1,0);CHECK(near((*b.numericRGBA)[0],244.0/360)&&near((*b.numericRGBA)[3],6.25));
 auto retained=b.numericRGBA;for(unsigned i=0;i<3000;++i)state->Update("LSDAmbience",{{"effectSpeed",0},{"waveFreq",100}},0,blue,1,0);CHECK(!state->Overflowed());CHECK(near((*retained)[3],6.25));CHECK(state->ReductionCount()==1);
 a=state->Update("LSDAmbience",{{"effectSpeed",100},{"waveFreq",100},{"colorMode","Custom"},{"color1","#ff0000"}},0,blue,1,0);CHECK(near((*a.numericRGBA)[0],0)&&near((*a.numericRGBA)[1],1)&&near((*a.numericRGBA)[2],.5));
 CHECK(!state->Update("LSDAmbience",{},0,QImage(),3,0).numericRGBA);CHECK(!state->Update("NotAScreenEffect",{},0,blue,3,0).numericRGBA);
 std::cout<<"PASS "<<checks<<" native screen state checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
