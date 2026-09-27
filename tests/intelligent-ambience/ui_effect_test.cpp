// SPDX-License-Identifier: GPL-2.0-or-later
// Real DLL, empty controller API, private temporary settings. GPU input is
// generated only by the effect's explicit demo mode; no live capture is used.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPluginLoader>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QEvent>
#include <functional>
#include <iostream>
#include <set>
#include "FakeAPI.h"

using json=nlohmann::json;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(#x);}while(0)

class OfflineAPI final : public FakeAPI
{
    filesystem::path directory;
public:
    explicit OfflineAPI(const QString& path):directory(path.toStdString()){}
    filesystem::path GetConfigurationDirectory() override{return directory;}
    json GetSettings(std::string) override{return json::object();}
};

static json Entry(const std::string& type,const json& custom,bool active=false)
{
    return {{"EffectClassName",type},{"CustomName","Offline Intelligent Ambience test"},
        {"FPS",30},{"Speed",1000},{"Slider2Val",1},{"RandomColors",false},
        {"AllowOnlyFirst",false},{"Brightness",100},{"Temperature",0},{"Tint",0},
        {"UserColors",json::array({0xffffff})},{"AutoStart",active},{"SelectAll",false},
        {"ControllerZones",json::array()},{"CustomSettings",custom}};
}
static void Load(OpenRGBPluginInterface* plugin,const json& entry)
{
    plugin->OnProfileAboutToLoad();
    plugin->OnProfileLoad({{"version",2},{"Effects",json::array({entry})}});
    QApplication::processEvents();
}
static json Save(OpenRGBPluginInterface* plugin)
{
    const auto saved=plugin->OnProfileSave();
    CHECK(saved.at("Effects").size()==1);
    return saved["Effects"][0];
}
static void EventsFor(int milliseconds)
{
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<milliseconds)
    {QApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(5);}
}
static bool Wait(const std::function<bool()>& ready,int milliseconds=10000)
{
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<milliseconds)
    {
        QApplication::processEvents(QEventLoop::AllEvents,10);
        if(ready())return true;
        QThread::msleep(5);
    }
    return false;
}
template<class T>static T* Field(QWidget* root,const char* name)
{auto* result=root->findChild<T*>(name);CHECK(result);return result;}
static QImage Preview(QWidget* root)
{
    auto* preview=Field<QLabel>(root,"preview");
    return preview->pixmap().toImage().convertToFormat(QImage::Format_RGB32);
}
static std::uint64_t ImageHash(const QImage& image)
{
    std::uint64_t hash=1469598103934665603ULL;
    for(int y=0;y<image.height();y+=3)for(int x=0;x<image.width();x+=3)
    {hash^=image.pixel(x,y);hash*=1099511628211ULL;}
    return hash;
}
static unsigned Diversity(const QImage& image)
{
    std::set<QRgb> colors;
    for(int y=0;y<image.height();y+=3)for(int x=0;x<image.width();x+=3)
        colors.insert(image.pixel(x,y));
    return unsigned(colors.size());
}
static void LegacyPresets(OpenRGBPluginInterface* plugin)
{
    const std::set<std::string> expected={"Aurora","CustomSpiral","Galaxies","GradientWave","Gradient",
        "RainbowRise","RainbowTunnel","Rainbow","SideToSide","SolidColor","Space","SpiralRainbow","Underwater",
        "RainbowTap","Terminal","NeonNebula","GoodNight","ColorCycle","NeonShift","PoliceLights",
        "RainbowPulse","ColorShift","TVStatic","CustomSunrise","CrookedWaves","QuadColorBreath","PumpUpBeats",
        "AverageColor","ScreenAmbience","LSDAmbience","Visor","CustomWave","Pinwheel","Spin","Plasma"};
    const QDir resources(":/Effects/SignalFavorites/presets");
    std::set<std::string> found;
    for(const auto& file:resources.entryList({"*.json"},QDir::Files,QDir::Name))
    {
        QFile source(resources.filePath(file));CHECK(source.open(QIODevice::ReadOnly));
        const auto spec=json::parse(source.readAll().toStdString());
        const std::string id=spec.at("id");CHECK(found.insert(id).second);
    }
    for(const auto& id:expected)
    {
        CHECK(found.count(id)==1);
        Load(plugin,Entry("SignalFavorite."+id,{{"use_audio",false}}));
        const auto saved=Save(plugin);
        CHECK(saved["EffectClassName"]=="SignalFavorite."+id);
        CHECK(saved["CustomSettings"]["preset"]==id);
        CHECK(saved["AutoStart"]==false);
        CHECK(!saved["CustomSettings"].contains("shader_program"));
    }
    CHECK(expected.size()==35);
}

int main(int argc,char** argv)
{
    // The Windows platform supports the real production OpenGL worker. The
    // widget is marked DontShowOnScreen, so no user-facing window is created.
    QApplication app(argc,argv);
    QTemporaryDir directory;
    std::unique_ptr<OfflineAPI> api;
    std::unique_ptr<QPluginLoader> loader;
    OpenRGBPluginInterface* plugin=nullptr;
    try
    {
        CHECK(argc==2);CHECK(directory.isValid());
        api=std::make_unique<OfflineAPI>(directory.path());
        loader=std::make_unique<QPluginLoader>(QString::fromLocal8Bit(argv[1]));
        auto* object=loader->instance();if(!object)throw std::runtime_error(loader->errorString().toStdString());
        plugin=qobject_cast<OpenRGBPluginInterface*>(object);CHECK(plugin&&plugin->GetPluginAPIVersion()==5);
        plugin->Load(api.get());QWidget* root=plugin->GetWidget();CHECK(root);
        root->setAttribute(Qt::WA_DontShowOnScreen,true);
        LegacyPresets(plugin);

        const std::string scene="c2fa7a84-5160-4e1b-a736-a0685d0ad8c0";
        // Never discover the personal connection file, even if the profile's
        // source controls become visible during an isolated UI test.
        const json screen={{"kind","better"},{"connection_file",(directory.path()+"/missing/connection.json").toStdString()},
            {"scene",scene},{"follow_better_appearance",false}};
        json custom={{"width",800},{"height",500},{"use_audio",false},{"screen_source",screen},
            {"intelligent",{{"mode",0},{"demo",false},{"predictive",true},{"persistence",1.0},
                {"strength",.75},{"screen",json::array({80,55,160,90})},{"hybrid",.25}}}};
        Load(plugin,Entry("IntelligentAmbience",custom));
        auto saved=Save(plugin);CHECK(saved["EffectClassName"]=="IntelligentAmbience");
        auto settings=saved["CustomSettings"];
        CHECK(settings["intelligent"]==custom["intelligent"]);
        CHECK(settings["width"]==800&&settings["height"]==500);
        CHECK(settings["screen_source"]["kind"]=="better");
        CHECK(settings["screen_source"]["scene"]==scene);
        CHECK(settings["screen_source"]["connection_file"]==screen["connection_file"]);
        CHECK(!settings.contains("shader_program")&&!settings.contains("shader_name"));
        CHECK(!root->findChild<QCheckBox*>("screen_source_follow_appearance"));
        CHECK(Field<QComboBox>(root,"ia_mode")->currentIndex()==0);
        CHECK(!Field<QCheckBox>(root,"ia_demo")->isChecked());
        CHECK(Field<QCheckBox>(root,"ia_predictive")->isChecked());
        CHECK(Field<QDoubleSpinBox>(root,"ia_persistence")->value()==1);
        CHECK(Field<QDoubleSpinBox>(root,"ia_strength")->value()==.75);
        CHECK(Field<QDoubleSpinBox>(root,"ia_hybrid")->value()==.25);
        const char* axes[]={"ia_screen_x","ia_screen_y","ia_screen_w","ia_screen_h"};
        const double initial[]={80,55,160,90};
        for(unsigned i=0;i<4;++i)CHECK(Field<QDoubleSpinBox>(root,axes[i])->value()==initial[i]);
        Field<QLabel>(root,"ia_status");
        bool category=false;
        for(auto* menu:root->findChildren<QMenu*>())if(menu->title()=="Advanced")
            for(auto* action:menu->actions())if(action->text().contains("Intelligent",Qt::CaseInsensitive))category=true;
        CHECK(category);

        // The existing panel owns the controls. Changing it must be reflected in
        // the normal Effects profile and survive a complete effect reconstruction.
        Field<QComboBox>(root,"ia_mode")->setCurrentIndex(2);
        Field<QCheckBox>(root,"ia_demo")->setChecked(true);
        Field<QCheckBox>(root,"ia_predictive")->setChecked(false);
        Field<QDoubleSpinBox>(root,"ia_persistence")->setValue(1.7);
        Field<QDoubleSpinBox>(root,"ia_strength")->setValue(.6);
        Field<QDoubleSpinBox>(root,"ia_hybrid")->setValue(.4);
        const double adjusted[]={63.25,42.5,171.5,96.25};
        for(unsigned i=0;i<4;++i)Field<QDoubleSpinBox>(root,axes[i])->setValue(adjusted[i]);
        saved=Save(plugin);const auto roundtrip=saved["CustomSettings"];
        CHECK(roundtrip["intelligent"]["mode"]==2&&roundtrip["intelligent"]["demo"]==true);
        CHECK(roundtrip["intelligent"]["predictive"]==false);
        CHECK(roundtrip["intelligent"]["persistence"]==1.7);
        for(unsigned i=0;i<4;++i)CHECK(roundtrip["intelligent"]["screen"][i]==adjusted[i]);
        Load(plugin,saved);CHECK(Save(plugin)["CustomSettings"]==roundtrip);
        QEvent language(QEvent::LanguageChange);QApplication::sendEvent(root,&language);
        CHECK(Save(plugin)["EffectClassName"]=="IntelligentAmbience");

        // Malformed sizes are not permitted to damage the last valid canvas.
        auto invalid=roundtrip;invalid["width"]=-1;invalid["height"]=0;
        Load(plugin,Entry("IntelligentAmbience",invalid));
        auto bounded=Save(plugin)["CustomSettings"];
        CHECK(bounded["width"].get<int>()>0&&bounded["height"].get<int>()>0);
        CHECK(std::uint64_t(bounded["width"].get<unsigned>())*bounded["height"].get<unsigned>()<=8ULL*1024*1024);

        auto malformed=custom;
        malformed["intelligent"]={{"mode","wrong type"},{"demo","true"},{"predictive",3},
            {"persistence",-500},{"strength",500},{"hybrid","wrong type"},
            {"screen",json::array({"wrong type",0,-20,999999})}};
        malformed["width"]="wrong type";malformed["height"]=500;
        Load(plugin,Entry("IntelligentAmbience",malformed));
        const auto repaired=Save(plugin)["CustomSettings"];
        CHECK(repaired["width"]==800&&repaired["height"]==500);
        CHECK(repaired["intelligent"]["mode"]==0&&repaired["intelligent"]["demo"]==false);
        CHECK(repaired["intelligent"]["predictive"]==true);
        CHECK(repaired["intelligent"]["persistence"]==.1&&repaired["intelligent"]["strength"]==1);
        CHECK(repaired["intelligent"]["hybrid"]==.25);
        CHECK(repaired["intelligent"]["screen"]==json::array({80,0,1,4}));
        malformed["intelligent"]=json::array({1,2,3});
        Load(plugin,Entry("IntelligentAmbience",malformed));
        CHECK(Save(plugin)["CustomSettings"]["intelligent"]==custom["intelligent"]);

        // A shipped native effect must not accept an editable fragment shader.
        const std::string injected="void mainImage(out vec4 c,in vec2 p){c=vec4(1,0,1,1);}";
        json program={{"version","110"},{"width",800},{"height",500},{"passes",json::array()},
            {"main_pass",{{"type",2},{"fragment_shader",injected},{"texture_path",""}}}};
        custom["shader_program"]=program;custom["shader_name"]="injected-profile";
        custom["intelligent"]["demo"]=true;custom["intelligent"]["mode"]=0;
        custom["show_rendering"]=true;custom["use_audio"]=false;
        Load(plugin,Entry("IntelligentAmbience",custom));
        saved=Save(plugin);CHECK(!saved["CustomSettings"].contains("shader_program"));
        CHECK(!saved["CustomSettings"].contains("shader_name"));
        CHECK(saved["CustomSettings"]["use_audio"]==false);
        saved["AutoStart"]=true;
        unsigned ticks=0;QTimer heartbeat;QObject::connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});heartbeat.start(10);
        root->resize(900,900);root->show();
        Load(plugin,saved);
        CHECK(Wait([&]{const auto image=Preview(root);return !image.isNull()&&Diversity(image)>4;}));
        const auto first=Preview(root);CHECK(first.width()>0&&first.height()>0);
        const auto hash=ImageHash(first);
        CHECK(Wait([&]{const auto image=Preview(root);return !image.isNull()&&ImageHash(image)!=hash;}));
        CHECK(ticks>=3);CHECK(Save(plugin)["AutoStart"]==true);
        CHECK(api->physical.empty()&&api->created.empty()&&api->attachments==0);
        // Stop and recreate without starting: the prior worker/image may not
        // leak into the newly stopped instance. Repeat the active lifecycle once.
        auto stopped=Save(plugin);stopped["AutoStart"]=false;
        Load(plugin,stopped);EventsFor(150);
        CHECK(Save(plugin)["AutoStart"]==false);CHECK(Preview(root).isNull());
        stopped["AutoStart"]=true;Load(plugin,stopped);
        CHECK(Wait([&]{const auto image=Preview(root);return !image.isNull()&&Diversity(image)>4;}));
        stopped["AutoStart"]=false;Load(plugin,stopped);EventsFor(100);
        CHECK(Save(plugin)["AutoStart"]==false);
        heartbeat.stop();root->hide();
        plugin->Unload();plugin=nullptr;CHECK(loader->unload());
        std::cout<<checks<<" real DLL/UI/persistence/demo GPU checks PASS; 35 legacy presets, empty controller API, no live source\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr<<"CHECK "<<checks<<": "<<error.what()<<'\n';
        if(plugin)try{plugin->Unload();}catch(...){ }
        return 1;
    }
}
