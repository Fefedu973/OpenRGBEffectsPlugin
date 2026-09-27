// SPDX-License-Identifier: GPL-2.0-or-later
// Real Effects DLL + Qt widgets, empty controller list, never starts an effect.
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPluginLoader>
#include <QTemporaryDir>
#include <QTimer>
#include <QColor>
#include <QComboBox>
#include <QLineEdit>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QEvent>
#include <iostream>
#include <set>
#include <map>
#include "FakeAPI.h"

using json=nlohmann::json;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(#x);}while(0)
class UIApi : public FakeAPI
{
    filesystem::path directory;
public:
    explicit UIApi(const QString& path):directory(path.toStdString()){}
    filesystem::path GetConfigurationDirectory()override{return directory;}
    json GetSettings(std::string)override{return json::object();}
};
static json Entry(const std::string& type, const json& custom)
{
    return {{"EffectClassName",type},{"CustomName","Offline UI test"},
        {"FPS",30},{"Speed",500},{"Slider2Val",1},{"RandomColors",false},
        {"AllowOnlyFirst",false},{"Brightness",70},{"Temperature",0},{"Tint",0},
        {"UserColors",json::array({0xffffff})},{"AutoStart",false},{"SelectAll",false},
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
    CHECK(saved["Effects"][0]["AutoStart"]==false);
    return saved["Effects"][0];
}
int main(int argc,char** argv)
{
    qputenv("QT_QPA_PLATFORM","offscreen");QApplication app(argc,argv);
    try
    {
        CHECK(argc==2);QTemporaryDir directory;CHECK(directory.isValid());
        UIApi api(directory.path());QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
        auto* object=loader.instance();if(!object)throw std::runtime_error(loader.errorString().toStdString());
        auto* plugin=qobject_cast<OpenRGBPluginInterface*>(object);CHECK(plugin&&plugin->GetPluginAPIVersion()==5);
        plugin->Load(&api);QWidget* root=plugin->GetWidget();CHECK(root);
        const QDir presets(":/Effects/SignalFavorites/presets");
        const auto files=presets.entryList({"*.json"},QDir::Files,QDir::Name);
        const std::set<std::string> expected={"Aurora","CustomSpiral","Galaxies","GradientWave","Gradient",
            "RainbowRise","RainbowTunnel","Rainbow","SideToSide","SolidColor","Space","SpiralRainbow","Underwater",
            "RainbowTap","Terminal","NeonNebula","GoodNight","ColorCycle","NeonShift","PoliceLights",
            "RainbowPulse","ColorShift","TVStatic","CustomSunrise","CrookedWaves","QuadColorBreath","PumpUpBeats",
            "AverageColor","ScreenAmbience","LSDAmbience","Visor","CustomWave","Pinwheel","Spin","Plasma"};
        std::set<std::string> found;
        CHECK(files.size()>=int(expected.size()));
        std::map<std::string,unsigned> category_sizes;
        for(const auto& file:files)
        {
            QFile source(presets.filePath(file));CHECK(source.open(QIODevice::ReadOnly));
            const auto spec=json::parse(source.readAll().toStdString());
            const std::string id=spec.at("id");
            ++category_sizes[spec.value("category",std::string("SignalRGB Favorites"))];
            CHECK(found.insert(id).second);
            Load(plugin,Entry("SignalFavorite."+id,json::object()));
            const auto saved_preset=Save(plugin);
            CHECK(saved_preset["EffectClassName"]=="SignalFavorite."+id);
            CHECK(saved_preset["CustomSettings"]["preset"]==id);
            CHECK(saved_preset["CustomSettings"]["parameters"].size()==spec.at("controls").size());
            CHECK(!saved_preset["CustomSettings"].contains("shader_program"));
            QEvent language(QEvent::LanguageChange);
            QApplication::sendEvent(root,&language);
            CHECK(Save(plugin)["EffectClassName"]=="SignalFavorite."+id);
            Load(plugin,saved_preset);
            CHECK(Save(plugin)["CustomSettings"]["parameters"]==saved_preset["CustomSettings"]["parameters"]);
        }
        for(const auto& category:category_sizes)
        {
            QMenu* category_menu=nullptr;
            for(auto* menu:root->findChildren<QMenu*>())
                if(menu->title().toStdString()==category.first){category_menu=menu;break;}
            CHECK(category_menu&&category_menu->actions().size()==int(category.second));
        }
        for(const auto& id:expected)CHECK(found.count(id)==1);
        // Source controls persist without connecting, capturing, or mutating Better.
        Load(plugin,Entry("SignalFavorite.ScreenAmbience",{{"screen_source",{{"kind","surface"},{"channel","synthetic-input-test"}}}}));
        auto* source_kind=root->findChild<QComboBox*>("screen_source_kind");
        auto* source_channel=root->findChild<QLineEdit*>("screen_source_channel");
        CHECK(source_kind && source_kind->currentData()=="surface");
        CHECK(source_channel && source_channel->text()=="synthetic-input-test");
        const auto source_saved=Save(plugin);Load(plugin,source_saved);
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]==source_saved["CustomSettings"]["screen_source"]);
        Load(plugin,Entry("SignalFavorite.ScreenAmbience",{{"screen_source",{{"kind","better"},{"channel","../invalid"}}}}));
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]["channel"]=="better-screen-capture");
        Load(plugin,Entry("SignalFavorite.ScreenAmbience",{{"screen_source",{{"kind","better"},{"connection_file","synthetic-test-profile/connection.json"}}}}));
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]["kind"]=="better");
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]["connection_file"]=="synthetic-test-profile/connection.json");
        auto* appearance=root->findChild<QCheckBox*>("screen_source_follow_appearance");
        CHECK(appearance&&appearance->isChecked());
        appearance->setChecked(false);
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]["follow_better_appearance"]==false);
        Load(plugin,Entry("SignalFavorite.ScreenAmbience",{{"screen_source",{{"kind","better"},{"connection_file","synthetic-test-profile/connection.json"},{"scene","f717b624-8acb-48e7-8db3-30bd99d1c07a"},{"follow_better_appearance",false}}}}));
        const auto better_saved=Save(plugin);Load(plugin,better_saved);
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]==better_saved["CustomSettings"]["screen_source"]);
        Load(plugin,Entry("SignalFavorite.AverageColor",json::object()));
        CHECK(!root->findChild<QCheckBox*>("screen_source_follow_appearance"));
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]["follow_better_appearance"]==false);
        Load(plugin,Entry("SignalFavorite.RainbowTap",json::object()));
        auto* keyboard=root->findChild<QCheckBox*>("keyboard_reactive");
        CHECK(keyboard&&keyboard->isChecked());
        keyboard->setChecked(false);
        auto keyboard_saved=Save(plugin);
        CHECK(keyboard_saved["CustomSettings"]["keyboard_reactive"]==false);
        Load(plugin,keyboard_saved);
        keyboard=root->findChild<QCheckBox*>("keyboard_reactive");
        CHECK(keyboard&&!keyboard->isChecked());
        keyboard->setChecked(true);
        CHECK(Save(plugin)["CustomSettings"]["keyboard_reactive"]==true);
        Load(plugin,Entry("SignalFavorite.PumpUpBeats",json::object()));
        auto* audio=root->findChild<QCheckBox*>("use_audio");
        CHECK(audio&&audio->isChecked());
        CHECK(Save(plugin)["CustomSettings"]["use_audio"]==true);
        auto* pump_style=root->findChild<QComboBox*>("colorStyle");
        auto* pump_source=root->findChild<QComboBox*>("screen_source_kind");
        CHECK(pump_style&&pump_style->currentText()=="HueCycle");
        CHECK(pump_style->count()==6&&pump_style->findText("ScreenDominant")==5);
        CHECK(pump_source&&pump_source->parentWidget()->isHidden());
        CHECK(!root->findChild<QCheckBox*>("screen_source_follow_appearance"));
        pump_style->setCurrentText("ScreenDominant");
        CHECK(!pump_source->parentWidget()->isHidden());
        auto palette_saved=Save(plugin);
        CHECK(palette_saved["CustomSettings"]["parameters"]["colorStyle"]=="ScreenDominant");
        CHECK(palette_saved["CustomSettings"]["screen_source"]["follow_better_appearance"]==false);
        // A copied ScreenAmbience flag must never replace the Pump render graph.
        palette_saved["CustomSettings"]["screen_source"]["follow_better_appearance"]=true;
        Load(plugin,palette_saved);
        CHECK(Save(plugin)["CustomSettings"]["screen_source"]["follow_better_appearance"]==false);
        pump_style=root->findChild<QComboBox*>("colorStyle");pump_source=root->findChild<QComboBox*>("screen_source_kind");
        CHECK(pump_style->currentText()=="ScreenDominant"&&!pump_source->parentWidget()->isHidden());
        pump_style->setCurrentText("RandomBeat");CHECK(pump_source->parentWidget()->isHidden());
        audio=root->findChild<QCheckBox*>("use_audio");
        audio->setChecked(false);
        auto audio_saved=Save(plugin);
        CHECK(audio_saved["CustomSettings"]["use_audio"]==false);
        Load(plugin,audio_saved);
        audio=root->findChild<QCheckBox*>("use_audio");
        CHECK(audio&&!audio->isChecked());
        Load(plugin,Entry("SignalFavorite.SolidColor",{{"parameters",{{"speed",99999},{"breathe","bad"},{"color","not-color"},{"unknown",9}}}}));
        auto saved=Save(plugin);const auto settings=saved["CustomSettings"];
        CHECK(saved["EffectClassName"]=="SignalFavorite.SolidColor");
        CHECK(saved["Speed"]==1000);CHECK(settings["width"]==800&&settings["height"]==500);
        CHECK(settings["parameters"]["speed"]==100);CHECK(settings["parameters"]["breathe"]==true);
        CHECK(QColor(QString::fromStdString(settings["parameters"]["color"])).name()=="#00bbff");
        CHECK(!settings["parameters"].contains("unknown"));
        CHECK(!settings.contains("shader_program")&&!settings.contains("shader_name"));
        CHECK(settings["preset"]=="SolidColor");
        auto* speed=root->findChild<QDoubleSpinBox*>("speed");CHECK(speed&&speed->value()==100);
        auto* breathe=root->findChild<QCheckBox*>("breathe");CHECK(breathe&&breathe->isChecked());
        speed->setValue(12);breathe->setChecked(false);
        saved=Save(plugin);CHECK(saved["CustomSettings"]["parameters"]["speed"]==12);
        CHECK(saved["CustomSettings"]["parameters"]["breathe"]==false);
        Load(plugin,saved);auto roundtrip=Save(plugin);
        if(roundtrip["CustomSettings"]["parameters"]!=saved["CustomSettings"]["parameters"])
            std::cerr<<"Saved parameters: "<<saved["CustomSettings"]["parameters"].dump()<<"\nReloaded parameters: "<<roundtrip["CustomSettings"]["parameters"].dump()<<'\n';
        CHECK(roundtrip["CustomSettings"]["parameters"]==saved["CustomSettings"]["parameters"]);
        Load(plugin,Entry("SignalFavorite.Gradient",{{"parameters",{{"numColors",-10},{"startX",999},{"endY","bad"}}},{"width",320},{"height",200}}));
        saved=Save(plugin);CHECK(saved["CustomSettings"]["parameters"]["numColors"]==2);
        CHECK(saved["CustomSettings"]["parameters"]["startX"]==320);
        CHECK(saved["CustomSettings"]["parameters"]["endY"]==0);
        CHECK(saved["CustomSettings"]["width"]==320&&saved["CustomSettings"]["height"]==200);
        // Ordinary Shaders keeps its previous editable-program persistence.
        const std::string fragment="void mainImage(out vec4 c,in vec2 p){c=vec4(1,0,0,1);}";
        json program={{"version","110"},{"width",37},{"height",19},{"passes",json::array()},
            {"main_pass",{{"type",2},{"fragment_shader",fragment},{"texture_path",""}}}};
        Load(plugin,Entry("Shaders",{{"width",37},{"height",19},{"shader_program",program},{"use_audio",false}}));
        saved=Save(plugin);CHECK(saved["EffectClassName"]=="Shaders"&&saved["Speed"]==500);
        CHECK(saved["CustomSettings"]["shader_program"]["main_pass"]["fragment_shader"]==fragment);
        CHECK(saved["CustomSettings"]["width"]==37&&saved["CustomSettings"]["height"]==19);
        // Continuous rhythm is a persistent opt-in. Keep audio disabled here:
        // this exercises the actual widget/profile path without opening a stream.
        auto* rhythm=root->findChild<QCheckBox*>("rhythm_tracking");CHECK(rhythm&&!rhythm->isChecked());
        rhythm->setChecked(true);saved=Save(plugin);
        CHECK(saved["CustomSettings"]["rhythm_tracking"]==true);
        CHECK(saved["CustomSettings"]["use_audio"]==false);
        Load(plugin,saved);rhythm=root->findChild<QCheckBox*>("rhythm_tracking");
        CHECK(rhythm&&rhythm->isChecked());
        rhythm->setChecked(false);saved=Save(plugin);
        CHECK(saved["CustomSettings"]["rhythm_tracking"]==false);
        // Native favorite preview must use the same serialized setting as the base UI.
        Load(plugin,Entry("SignalFavorite.SolidColor",json::object()));
        QCheckBox* preview=nullptr;
        for(auto* box:root->findChildren<QCheckBox*>())
            if(box->text().contains("Preview (")){preview=box;break;}
        CHECK(preview);preview->setChecked(true);
        saved=Save(plugin);CHECK(saved["CustomSettings"]["show_rendering"]==true);
        Load(plugin,saved);
        preview=nullptr;
        for(auto* box:root->findChildren<QCheckBox*>())
            if(box->text().contains("Preview (")){preview=box;break;}
        CHECK(preview&&preview->isChecked());
        plugin->Unload();CHECK(loader.unload());
        std::cout<<checks<<" real DLL/UI/persistence checks PASS across "<<files.size()<<" native favorites; no controllers or effects started\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"CHECK "<<checks<<": "<<e.what()<<'\n';return 1;}
}
