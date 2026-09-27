// SPDX-License-Identifier: GPL-2.0-or-later
#include "Appearance.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace better_capture { namespace {
using json=nlohmann::json;
double Num(const json& j,const char* key,double fallback,double low,double high) {
    const auto it=j.find(key);if(it==j.end()||!it->is_number())return fallback;
    const double v=it->get<double>();return std::isfinite(v)?std::clamp(v,low,high):fallback;
}
bool Bool(const json& j,const char* key,bool fallback) {
    const auto it=j.find(key);return it!=j.end()&&it->is_boolean()?it->get<bool>():fallback;
}
std::string Text(const json& j,const char* key,const std::string& fallback) {
    const auto it=j.find(key);return it!=j.end()&&it->is_string()?it->get<std::string>():fallback;
}
void Put(Uniforms& u,const char* key,double a) {u[key]={{{float(a),0,0,0}},1};}
void Put(Uniforms& u,const char* key,double a,double b,double c,double d) {u[key]={{{float(a),float(b),float(c),float(d)}},4};}
struct Settings {
    double x,y,w,h,hue,brightness,saturation,blur,spread,glowSat,intensity,cutoff,edgeDepth,edgeMix,reach,fade;
    unsigned mode=0,style=0;
    bool pictureBlur,halo,fullscreen,hide,pixelated;
    explicit Settings(const json& j) {
        w=Num(j,"screenWidth",320,1,320);h=Num(j,"screenHeight",200,1,200);
        x=Num(j,"screenX",0,0,320-w);y=Num(j,"screenY",0,0,200-h);
        hue=Num(j,"hue",0,-180,180);brightness=1+Num(j,"brightness",0,-100,100)/100;
        saturation=1+Num(j,"saturation",0,-100,100)/100;
        blur=Num(j,"ambilightBlur",30,0,100);spread=Num(j,"ambilightSpread",10,0,100);
        glowSat=Num(j,"ambilightSaturation",3,0,10);intensity=Num(j,"ambilightIntensity",100,0,200)/100;
        cutoff=Num(j,"ambilightCutoff",0,0,100)/100;edgeDepth=Num(j,"ambilightEdgeDepth",3,1,20)/100;
        edgeMix=Num(j,"ambilightEdgeMix",2,0,30)/100;reach=Num(j,"ambilightEdgeReach",60,1,200);
        fade=Num(j,"ambilightEdgeFade",50,0,100)/100;
        pictureBlur=Bool(j,"blur",false);halo=Bool(j,"ambilight",true);fullscreen=Bool(j,"ambilightFullscreen",false);
        hide=Bool(j,"hideSources",false);pixelated=Text(j,"interpolation","smooth")=="pixelated";
        const auto m=Text(j,"pictureMode","Standard"),s=Text(j,"ambilightStyle","Classic");
        const char* modes[]={"Standard","Cinema","Mono","Vivid","Dominant","HD"};
        for(unsigned i=0;i<6;++i)if(m==modes[i])mode=i;
        style=s=="Soft"?1:(s=="Contours"?2:0);
    }
};

std::vector<int> NearestBoundary(const std::vector<unsigned char>& occupied,unsigned w,unsigned h) {
    const unsigned n=w*h;std::vector<unsigned char> seeds(n);std::vector<int> nearest(n,-1),rowSeed(n,-1);
    std::vector<double> distance(n);unsigned count=0;
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
        const auto i=y*w+x;if(occupied[i]&&(!x||!y||x==w-1||y==h-1||!occupied[i-1]||!occupied[i+1]||!occupied[i-w]||!occupied[i+w])){seeds[i]=1;++count;}
    }
    if(!count)return nearest;
    for(unsigned y=0;y<h;++y) {
        int last=-100000;for(unsigned x=0;x<w;++x){const auto i=y*w+x;if(seeds[i])last=int(x);distance[i]=std::pow(double(int(x)-last),2);rowSeed[i]=last>=0?last:-1;}
        last=100000;for(int x=int(w)-1;x>=0;--x){const auto i=y*w+x;if(seeds[i])last=x;const auto d=std::pow(double(x-last),2);if(d<distance[i]){distance[i]=d;rowSeed[i]=last<int(w)?last:-1;}}
    }
    std::vector<int> sites(h);std::vector<double> borders(h+1);
    for(unsigned x=0;x<w;++x) {
        int end=-1;for(unsigned y=0;y<h;++y) {
            if(rowSeed[y*w+x]<0)continue;double crossing=-std::numeric_limits<double>::infinity();
            while(end>=0){const int previous=sites[end];crossing=((distance[y*w+x]+double(y)*y)-(distance[previous*w+x]+double(previous)*previous))/(2*(int(y)-previous));if(crossing>borders[end])break;--end;}
            sites[++end]=int(y);borders[end]=end?crossing:-std::numeric_limits<double>::infinity();borders[end+1]=std::numeric_limits<double>::infinity();
        }
        int segment=0;for(unsigned y=0;y<h;++y){while(segment<end&&borders[segment+1]<y)++segment;const int row=sites[segment];nearest[y*w+x]=row*int(w)+rowSeed[row*w+x];}
    }
    return nearest;
}
double Continuous(const std::vector<unsigned char>& mask,unsigned w,unsigned h,double x,double y,double dx,double dy,double maximum) {
    int cx=int(std::floor(x)),cy=int(std::floor(y));
    auto inside=[&]{return cx>=0&&cy>=0&&cx<int(w)&&cy<int(h)&&mask[cy*w+cx];};
    if(!inside())return -1;
    const int ix=dx>0?1:-1,iy=dy>0?1:-1;
    const double infinity=std::numeric_limits<double>::infinity();
    const double deltaX=std::abs(dx)<1e-9?infinity:std::abs(1/dx),deltaY=std::abs(dy)<1e-9?infinity:std::abs(1/dy);
    double nextX=std::isfinite(deltaX)?(dx>0?cx+1-x:x-cx)*deltaX:infinity;
    double nextY=std::isfinite(deltaY)?(dy>0?cy+1-y:y-cy)*deltaY:infinity;
    for(unsigned i=0;i<=w+h;++i){const double next=std::min(nextX,nextY);if(next>=maximum)return maximum;
        if(nextX<=next){cx+=ix;nextX+=deltaX;}if(nextY<=next){cy+=iy;nextY+=deltaY;}
        if(!inside())return std::max(0.0,next-.03);
    }return 0;
}
void Geometry(Prepared& result,const json& sources,const Settings& s,unsigned outputWidth,unsigned outputHeight) {
    const unsigned w=std::min(320u,outputWidth),h=std::min(320u,std::max(1u,unsigned(std::floor(double(w)*outputHeight/outputWidth+.5))));
    const double metricW=320,metricH=320.0*outputHeight/outputWidth;
    result.silhouette=QImage(int(w),int(h),QImage::Format_ARGB32_Premultiplied);result.silhouette.fill(Qt::transparent);
    auto raster=[&](const QPainterPath& path){QImage mask(int(w),int(h),QImage::Format_ARGB32_Premultiplied);mask.fill(Qt::transparent);QPainter painter(&mask);painter.setRenderHint(QPainter::Antialiasing);painter.fillPath(path,Qt::white);return mask;};
    auto finite=[](const json& object,const char* key){const auto it=object.find(key);if(it==object.end()||!it->is_number()||!std::isfinite(it->get<double>()))throw std::invalid_argument("Malformed Better source geometry");return it->get<double>();};
    QPainterPath canvasPath;canvasPath.addRect(s.x*w/320,s.y*h/200,s.w*w/320,s.h*h/200);const auto canvasMask=raster(canvasPath);
    for(const auto& source:sources) {
        if(!source.is_object())throw std::invalid_argument("Malformed Better source");
        if(!Bool(source,"contributes",false))continue;
        const auto polygon=source.find("placedCoveragePolygon");if(polygon==source.end()||!polygon->is_array()||polygon->size()>16)throw std::invalid_argument("Malformed Better placed polygon");
        for(const auto& p:*polygon){const double x=finite(p,"x"),y=finite(p,"y");if(x<0||x>320||y<0||y>200)throw std::invalid_argument("Unclipped Better polygon");}
        const double sw=finite(source,"canvasWidth"),sh=finite(source,"canvasHeight");
        if(sw<=0||sh<=0||sw>32768||sh>32768)throw std::invalid_argument("Invalid Better source dimensions");
        const auto matrix=source.find("canvasFromImage"),crop=source.find("cropPolygonLocal");
        if(matrix==source.end()||!matrix->is_object()||crop==source.end()||!crop->is_array()||crop->size()>16)throw std::invalid_argument("Missing Better source transform");
        const double aa=finite(*matrix,"m11"),ab=finite(*matrix,"m12"),ba=finite(*matrix,"m21"),bb=finite(*matrix,"m22"),tx=finite(*matrix,"dx"),ty=finite(*matrix,"dy");
        if(std::max({std::abs(aa),std::abs(ab),std::abs(ba),std::abs(bb)})>16||std::abs(tx)>65536||std::abs(ty)>65536)throw std::invalid_argument("Excessive Better source transform");
        auto point=[&](double x,double y){return QPointF((s.x+(aa*x+ba*y+tx)*s.w/320)*w/320,(s.y+(ab*x+bb*y+ty)*s.h/200)*h/200);};
        QPolygonF outer;outer<<point(0,0)<<point(sw,0)<<point(sw,sh)<<point(0,sh);QPainterPath rectangle;rectangle.addPolygon(outer);rectangle.closeSubpath();
        QPolygonF cropped;for(const auto& p:*crop){const double x=finite(p,"x"),y=finite(p,"y");if(std::abs(x)>65536||std::abs(y)>65536)throw std::invalid_argument("Excessive Better crop");cropped<<point(x,y);}QPainterPath cropPath;cropPath.addPolygon(cropped);cropPath.closeSubpath();
        const auto rectangleMask=raster(rectangle),cropMask=raster(cropPath);
        // The WEB renderer clips by the source rectangle and crop, then fills
        // that same rectangle. Coverage at coincident antialiased edges is
        // multiplied, not replaced by the AA of a single polygon union.
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const double a=qAlpha(rectangleMask.pixel(x,y))/255.0,c=qAlpha(cropMask.pixel(x,y))/255.0,clip=qAlpha(canvasMask.pixel(x,y))/255.0;
            const double coverage=a*a*c*clip,old=qAlpha(result.silhouette.pixel(x,y))/255.0;const int value=int(std::round(255*(coverage+old*(1-coverage))));result.silhouette.setPixel(x,y,qRgba(value,value,value,value));}
    }
    std::vector<unsigned char> occupancy(w*h);bool opaque=true;
    for(unsigned i=0;i<w*h;++i){const int a=qAlpha(result.silhouette.pixel(int(i%w),int(i/w)));occupancy[i]=a>=128;opaque=opaque&&a==255;}
    const auto nearest=opaque?std::vector<int>(w*h,-1):NearestBoundary(occupancy,w,h);
    auto atlas=std::make_shared<std::vector<float>>(std::size_t(w)*h*8,0);
    const double size=std::min(s.w,s.h/200*metricH),depth=std::max(.5,size*s.edgeDepth),lateral=size*s.edgeMix;
    const double sx=w/metricW,sy=h/metricH;
    for(unsigned i=0;i<w*h;++i) {
        (*atlas)[i*4+2]=qAlpha(result.silhouette.pixel(int(i%w),int(i/w)))/255.f;
        (*atlas)[(w*h+i)*4+3]=float(occupancy[i]);
        if(nearest[i]<0)continue;const int qx=nearest[i]%int(w),qy=nearest[i]/int(w);
        (*atlas)[i*4]=float(std::round((qx+.5)/w*65535)/65535);(*atlas)[i*4+1]=float(std::round((qy+.5)/h*65535)/65535);(*atlas)[i*4+3]=1;
        if(qAlpha(result.silhouette.pixel(int(i%w),int(i/w)))==255)continue;
        const double dx=(qx-int(i%w))*metricW/w,dy=(qy-int(i/w))*metricH/h,d=std::hypot(dx,dy);
        const double nx=d?dx/d:0,ny=d?dy/d:1;
        for(int side=lateral<.001?0:-1;side<=(lateral<.001?0:1);++side){const double tx=-ny*side*sx,ty=nx*side*sy;
            if(side&&Continuous(occupancy,w,h,qx+.5,qy+.5,tx,ty,lateral)<lateral)continue;
            const double distance=Continuous(occupancy,w,h,qx+.5+tx*lateral,qy+.5+ty*lateral,nx*sx,ny*sy,depth);
            if(distance>=0)(*atlas)[(w*h+i)*4+side+1]=float((1+std::floor(std::min(1.0,distance/depth)*254))/255);
        }
    }
    result.geometryWidth=w;result.geometryHeight=h*2;result.geometryRGBA=atlas;
}
}

Prepared Prepare(const nlohmann::json& rendering,unsigned width,unsigned height) {
    if(!width||!height||width>4096||height>4096||std::uint64_t(width)*height>4194304)throw std::invalid_argument("Better output dimensions exceed bounds");
    if(!rendering.is_object()||rendering.value("version",0)!=1||rendering.value("schema",std::string())!="better.native-rendering"||rendering.value("canvasWidth",0)!=320||rendering.value("canvasHeight",0)!=200)throw std::invalid_argument("Unsupported Better rendering schema");
    if(!rendering.contains("effectiveSettings")||!rendering["effectiveSettings"].is_object()||!rendering.contains("sources")||!rendering["sources"].is_array()||rendering["sources"].size()>128)throw std::invalid_argument("Malformed Better appearance metadata");
    Settings s(rendering["effectiveSettings"]);Prepared result;Uniforms common;
    Put(common,"bPlacement",s.x/320,s.y/200,s.w/320,s.h/200);Put(common,"bTone",s.hue,s.brightness,s.saturation,s.mode);
    Put(common,"bGlow",s.glowSat,s.intensity,s.cutoff,s.style);Put(common,"bOptions",s.pictureBlur,s.halo,s.fullscreen,s.hide);
    Put(common,"bPixelated",s.pixelated);Put(common,"bOutputSize",width,height,320,200);
    auto add=[&](std::string id,std::string shader,std::array<std::string,4> inputs,unsigned w,unsigned h,Uniforms extra=Uniforms{}) {
        Pass p;p.id=std::move(id);p.shader="shaders/BetterCapture/"+shader+".fs";p.inputs=std::move(inputs);p.width=w;p.height=h;p.uniforms=common;
        std::array<float,4> flips{};for(unsigned i=0;i<4;++i)flips[i]=!p.inputs[i].empty()&&p.inputs[i]!="raw"&&p.inputs[i]!="coverage"&&p.inputs[i]!="geometry"?1.f:0.f;
        p.uniforms["bInputFlip"]={flips,4};for(const auto& x:extra)p.uniforms[x.first]=x.second;
        result.graph.passes.push_back(std::move(p));return result.graph.passes.back().id;
    };
    auto blur=[&](const std::string& id,const std::string& input,unsigned w,unsigned h,double sigmaX,double sigmaY){
        Uniforms ux,uy;Put(ux,"bKernel",sigmaX,1,0,0);Put(uy,"bKernel",sigmaY,0,1,0);
        auto x=add(id+"X","gaussian",{input,"","",""},w,h,ux);return add(id,"gaussian",{x,"","",""},w,h,uy);
    };
    std::string scene=add("tone","tone",{"raw","coverage","",""},width,height);
    if(s.pictureBlur)scene=blur("pictureBlur",scene,width,height,width/320.0,height/200.0);
    const auto picture=add("picture","place",{scene,"","",""},width,height);
    std::string glow,far;
    if(s.halo&&s.style==2) {
        Geometry(result,rendering["sources"],s,width,height);
        Uniforms u;const double mh=320.0*height/width,size=std::min(s.w,s.h/200*mh);
        Put(u,"bFieldSize",result.geometryWidth,result.geometryHeight/2,320,mh);
        Put(u,"bContour",std::max(.5,size*s.edgeDepth),size*s.edgeMix,s.fullscreen?std::hypot(320.0,mh):s.reach,s.fade);
        glow=add("contours","contours",{"raw","coverage","geometry",""},width,height,u);
    } else if(s.halo) {
        const double near=std::max(.01,s.blur*.35+s.spread*.15),distant=std::max(.01,s.blur+s.spread*.5);
        const unsigned margin=unsigned(std::ceil(s.style==1?3*std::max(near,distant):s.spread+3*s.blur));
        const unsigned gw=320+2*margin,gh=200+2*margin;Uniforms domain;
        Put(domain,"bDomain",margin,gw,gh,0);
        glow=add("glowSource","glow-source",{s.fullscreen?scene:picture,"","",""},gw,gh,domain);
        if(s.style==0) {
            unsigned remaining=unsigned(std::ceil(s.spread)),radius=0,stage=0;
            while(remaining){const unsigned step=std::min(remaining,2*radius+1);Uniforms u;Put(u,"bMorphStep",step);glow=add("dilate"+std::to_string(stage++),"dilate",{glow,"","",""},gw,gh,u);radius+=step;remaining-=step;}
            glow=add("glowSaturated","saturate",{glow,"","",""},gw,gh);
            glow=blur("classicBlur",glow,gw,gh,s.blur,s.blur);
        } else {
            auto input=glow;glow=blur("softNear",input,gw,gh,near,near);far=blur("softFar",input,gw,gh,distant,distant);
        }
        Put(common,"bDomain",margin,gw,gh,0);
    }
    result.graph.output=add("output","compose",{picture,glow,far,""},width,height);
    if(result.graph.passes.size()>32)throw std::invalid_argument("Better render graph exceeds bound");
    std::uint64_t bytes=0;for(const auto& pass:result.graph.passes)bytes+=std::uint64_t(pass.width)*pass.height*8;
    if(bytes>128ULL*1024*1024)throw std::invalid_argument("Better render graph exceeds intermediate memory budget");
    return result;
}
}
