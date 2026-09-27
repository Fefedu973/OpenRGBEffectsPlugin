// SPDX-License-Identifier: GPL-2.0-or-later
#include "SignalFavorite.h"
#include "SignalFavoriteRegistry.h"
#include "EffectListManager.h"
#include "ShaderCanvas.h"
#include "OpenRGBEffectsPlugin.h"
#include "PhysicalKeys.h"
#include <FrameRouting/RGBControllerInputMappingInterface.h>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <regex>

namespace
{
json ReadSpec(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing native favorite specification");
    return json::parse(file.readAll().toStdString());
}
void HideLayout(QLayout* layout)
{
    while(auto* item = layout->takeAt(0))
    {
        if(item->widget()) item->widget()->hide();
        if(item->layout()) HideLayout(item->layout());
        delete item;
    }
}
}

void RegisterSignalFavoritePresets() { SignalFavorite::RegisterPresets(); }

void SignalFavorite::RegisterPresets()
{
    static bool registered = false;
    if(registered) return;
    registered = true;
    const QDir directory(":/Effects/SignalFavorites/presets");
    for(const QString& filename : directory.entryList({"*.json"},QDir::Files,QDir::Name))
    {
        const QString path = directory.filePath(filename);
        try
        {
            const auto definition = ReadSpec(path);
            EffectListManager::get()->RegisterEffect("SignalFavorite." + definition.at("id").get<std::string>(),
                definition.at("title"), definition.value("category",std::string("SignalRGB Favorites")), [path](){ return new SignalFavorite(path); });
        }
        catch(const std::exception& ex) { qWarning() << "Native favorite:" << path << ex.what(); }
    }
}

SignalFavorite::SignalFavorite(const QString& resource, QWidget* parent) : Shaders(parent), spec(ReadSpec(resource))
{
    EffectDetails.EffectName = spec.at("title");
    EffectDetails.EffectClassName = "SignalFavorite." + spec.at("id").get<std::string>();
    EffectDetails.EffectDescription = spec.value("description",std::string("Native GPU animation"));
    EffectDetails.MinSpeed = EffectDetails.MaxSpeed = 0;
    EffectDetails.ExpandCustomSettings = true;
    for(const auto& control : spec.at("controls"))
    {
        const auto key = control.at("key").get<std::string>();
        if(!std::regex_match(key,std::regex("[A-Za-z_][A-Za-z0-9_]*"))) throw std::runtime_error("Invalid uniform key");
        parameters[key] = Normalize(control,control.at("default"));
    }

    // Preserve the Shaders widget's canvas routing and bounded preview timer;
    // replace only its generic editor controls with this preset's controls.
    preview = ShaderUi()->preview;
    HideLayout(layout());
    delete layout();
    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    outer->addLayout(form);
    for(const auto& control : spec.at("controls"))
    {
        const std::string key = control.at("key");
        const std::string type = control.at("type");
        QWidget* editor = nullptr;
        if(type == "number")
        {
            auto* field = new QDoubleSpinBox(this);
            field->setRange(control.value("min",0.0),control.value("max",100.0));
            field->setSingleStep(control.value("step",1.0));
            field->setDecimals(control.value("step",1.0) < 1.0 ? 3 : 0);
            connect(field,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,key](double v){SetParameter(key,v);});
            editor = field;
        }
        else if(type == "boolean")
        {
            auto* field = new QCheckBox(this);
            connect(field,&QCheckBox::toggled,this,[this,key](bool v){SetParameter(key,v);});
            editor = field;
        }
        else if(type == "enum")
        {
            auto* field = new QComboBox(this);
            for(const auto& choice : control.at("options")) field->addItem(QString::fromStdString(choice));
            connect(field,&QComboBox::currentTextChanged,this,[this,key](const QString& v){SetParameter(key,v.toStdString());});
            editor = field;
        }
        else if(type == "color")
        {
            auto* field = new QPushButton(this);
            connect(field,&QPushButton::clicked,this,[this,key,field]{
                QColor current;
                { std::lock_guard<std::mutex> lock(parameters_mutex); current = QColor(QString::fromStdString(parameters.at(key))); }
                const QColor chosen = QColorDialog::getColor(current,this,tr("Select color"));
                if(chosen.isValid()) { SetParameter(key,chosen.name().toStdString()); field->setText(chosen.name()); field->setStyleSheet("background-color:"+chosen.name()+";"); }
            });
            editor = field;
        }
        if(editor)
        {
            editor->setObjectName(QString::fromStdString(key));
            editors[key] = editor;
            form->addRow(QString::fromStdString(control.at("label")),editor);
        }
    }
    auto* size_row = new QWidget(this);
    auto* size_layout = new QHBoxLayout(size_row);
    size_layout->setContentsMargins(0,0,0,0);
    canvas_width = new QSpinBox(size_row); canvas_height = new QSpinBox(size_row);
    for(auto* field : {canvas_width,canvas_height}) { field->setRange(1,4096); size_layout->addWidget(field); }
    canvas_width->setValue(800); canvas_height->setValue(500);
    form->addRow(tr("Canvas resolution"),size_row);
    auto resize_canvas = [this]{
        if(!ShaderCanvas::ValidSize(canvas_width->value(),canvas_height->value())) return;
        auto settings = Shaders::SaveCustomSettings();
        settings.erase("shader_program"); settings.erase("shader_name");
        settings["width"] = canvas_width->value(); settings["height"] = canvas_height->value();
        Shaders::LoadCustomSettings(settings);
    };
    connect(canvas_width,qOverload<int>(&QSpinBox::valueChanged),this,resize_canvas);
    connect(canvas_height,qOverload<int>(&QSpinBox::valueChanged),this,resize_canvas);
    auto* show = ShaderUi()->show_rendering;
    show->setText(tr("Preview (click to trigger tap effects)"));
    outer->addWidget(show); outer->addWidget(preview);
    show->show();
    preview->setMinimumSize(320,200);
    preview->setAlignment(Qt::AlignCenter);
    preview->installEventFilter(this);
    connect(show,&QCheckBox::toggled,preview,&QWidget::setVisible);
    preview->hide();
    if(spec.value("audioReactive",false))
    {
        outer->addWidget(ShaderUi()->use_audio);
        outer->addWidget(ShaderUi()->audio_settings);
        ShaderUi()->use_audio->show();
    }
    if(spec.contains("tap_speed_key")||spec.value("keyboard_reactive",false))
    {
        keyboard_checkbox=new QCheckBox(tr("React to physical keyboard keys"),this);
        keyboard_checkbox->setObjectName("keyboard_reactive");
        keyboard_checkbox->setChecked(true);
        outer->addWidget(keyboard_checkbox);
        auto* input_status=new QLabel(this); input_status->setWordWrap(true); outer->addWidget(input_status);
        connect(keyboard_checkbox,&QCheckBox::toggled,this,[this](bool enabled){
            {std::lock_guard<std::mutex> guard(parameters_mutex);keyboard_enabled=enabled;}
            UpdateInputListener();
        });
        auto* status_timer=new QTimer(this);
        connect(status_timer,&QTimer::timeout,this,[this,input_status]{
            if(!isVisible()) return;
            std::lock_guard<std::mutex> guard(parameters_mutex);
            input_status->setText(input_listener?tr("Keyboard input active. Physical positions follow the assigned layout."):
                !effect_enabled?tr("Keyboard input starts with this effect."):
                !keyboard_enabled?tr("Keyboard input disabled."):tr("Keyboard input unavailable in this host. Preview clicks remain available."));
        });
        status_timer->start(500);
    }
    const QString notes = QString::fromStdString(spec.value("notes",std::string()));
    if(!notes.isEmpty()) { auto* label = new QLabel(notes,this); label->setWordWrap(true); outer->addWidget(label); }
    SyncEditors();
    InstallProgram(resource);
    Shaders::LoadCustomSettings({{"width",800},{"height",500},{"use_audio",spec.value("audioReactive",false)}});
    SetFPS(60);
    StepEffect({}); // Initialize uniform defaults before the first GPU frame.
}

SignalFavorite::~SignalFavorite() { EffectState(false); }

json SignalFavorite::Normalize(const json& control, const json& value) const
{
    const std::string type = control.at("type");
    const auto fallback = control.at("default");
    if(type == "number")
    {
        double v = value.is_number() ? value.get<double>() : fallback.get<double>();
        if(!std::isfinite(v)) v = fallback.get<double>();
        return std::clamp(v,control.value("min",0.0),control.value("max",100.0));
    }
    if(type == "boolean") return value.is_boolean() ? value : fallback;
    if(type == "enum")
    {
        const auto& choices = control.at("options");
        return value.is_string() && std::find(choices.begin(),choices.end(),value)!=choices.end() ? value : fallback;
    }
    if(type == "color")
    {
        const QColor color(value.is_string() ? QString::fromStdString(value.get<std::string>()) : QString());
        const QColor normalized = color.isValid() ? color : QColor(QString::fromStdString(fallback.get<std::string>()));
        return normalized.name().toStdString();
    }
    return fallback;
}

void SignalFavorite::SetParameter(const std::string& key,const json& value)
{
    std::lock_guard<std::mutex> guard(parameters_mutex);
    for(const auto& control : spec.at("controls")) if(control.at("key") == key)
    { parameters[key] = Normalize(control,value); break; }
}

void SignalFavorite::SyncEditors()
{
    std::lock_guard<std::mutex> guard(parameters_mutex);
    for(const auto& entry : editors)
    {
        const QSignalBlocker blocker(entry.second);
        const auto& value = parameters.at(entry.first);
        if(auto* number = qobject_cast<QDoubleSpinBox*>(entry.second)) number->setValue(value.get<double>());
        else if(auto* boolean = qobject_cast<QCheckBox*>(entry.second)) boolean->setChecked(value.get<bool>());
        else if(auto* combo = qobject_cast<QComboBox*>(entry.second)) combo->setCurrentText(QString::fromStdString(value));
        else if(auto* color = qobject_cast<QPushButton*>(entry.second))
        { color->setText(QString::fromStdString(value)); color->setStyleSheet("background-color:"+color->text()+";"); }
    }
}

void SignalFavorite::InstallProgram(const QString& resource)
{
    const QString basename = QFileInfo(resource).completeBaseName();
    QFile shader(":/shaders/SignalFavorites/"+basename+".fs");
    if(!shader.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing native favorite shader");
    std::string prefix = "uniform vec4 iTap;\nuniform float iTapCount;\nuniform vec4 iTapEvents[64];\nuniform vec4 iTapMeta[64];\n";
    prefix+=native_basic::State::Declarations(spec.at("id"));
    for(const auto& control : spec.at("controls"))
    {
        const std::string key = control.at("key");
        const std::string type = control.at("type");
        prefix += "uniform " + std::string(type=="color"?"vec3":"float") + " p_" + key + ";\n";
        if(type == "number") prefix += "uniform float t_" + key + ";\n";
    }
    // Called only during construction, before Start: retain the base editor's
    // program instead of abandoning an allocated program on every preset load.
    auto* program = Renderer()->Program();
    program->SetVersion("130"); program->Resize(800,500);
    program->main_pass->data.feedback=spec.value("feedback",false);
    program->main_pass->data.fragment_shader = prefix + shader.readAll().toStdString();
    program->recompile = true;
}

void SignalFavorite::EffectState(bool enabled)
{
    {
        std::lock_guard<std::mutex> guard(parameters_mutex);
        clock_running = false;
        effect_enabled=enabled;
        tap_history.Clear(); tap={0,0,0,0};
        basic_state.Reset();
        pump_state.Reset();pending_helper_taps=0;
    }
    UpdateInputListener();
    // Profiles load controls after construction. Prime those values while the
    // renderer is stopped so its first frame cannot flash the factory colors.
    if(enabled && !Renderer()->isRunning()) StepEffect({});
    Shaders::EffectState(enabled);
}

void SignalFavorite::UpdateInputListener()
{
    std::lock_guard<std::mutex> guard(parameters_mutex);
    const bool wanted=effect_enabled&&keyboard_enabled&&keyboard_checkbox;
    if(!wanted)
    {
        if(input_api&&input_listener) input_api->ReleaseKeyboardInput(input_listener);
        input_listener=0; input_api=nullptr; input_identity.Clear(); input_deduplication.Clear(); tap_history.Clear();pending_helper_taps=0;
    }
    else if(!input_listener)
    {
        input_api=dynamic_cast<room_input::PluginAPI*>(OpenRGBEffectsPlugin::api);
        if(input_api&&input_api->InputAPIVersion()==1) input_listener=input_api->AcquireKeyboardInput();
    }
}

void SignalFavorite::OnControllerZonesListChanged(std::vector<ControllerZone*> zones)
{
    Shaders::OnControllerZonesListChanged(zones);
    std::lock_guard<std::mutex> guard(parameters_mutex);
    tap_history.Clear();tap={0,0,0,0};input_identity.Clear();input_deduplication.Clear();pending_helper_taps=0;
    if(input_api&&input_listener) input_api->ReadKeyboardInput(input_listener);
}

void SignalFavorite::CollectKeyboardTaps(const std::vector<ControllerZone*>& zones,double speed,double now)
{
    if(!input_api||!input_listener) return;
    const auto events=input_api->ReadKeyboardInput(input_listener);
    if(events.empty()) return;
    const auto regions=CanvasRegions();
    std::vector<room_input::InputPoint> points;
    for(auto* zone:zones)
    {
        if(!zone||!zone->controller||!zone->self_brightness) continue;
        const auto region=effect_canvas::RegionFor(zone,regions);
        if(auto* mapped=dynamic_cast<room_input::RGBControllerInputMappingInterface*>(zone->controller))
        {
            if(zone->is_segment) continue;
            std::vector<room_input::InputPoint> members;
            if(!mapped->GetInputPoints(zone->zone_idx,members)) continue;
            const auto transform=room_image::Mapping::Rectangle(region.x,region.y,region.width,region.height,
                region.rotation,region.flip_x!=zone->reverse,region.flip_y);
            for(auto& member:members)
            {
                double x,y;
                if(points.size()>=room_input::MaxInputPoints) break;
                if(transform.Point(member.u,member.v,x,y)&&x>=0&&x<=1&&y>=0&&y<=1)
                {member.u=x;member.v=y;points.push_back(std::move(member));}
            }
        }
        else if(zone->controller->GetDeviceType()==DEVICE_TYPE_KEYBOARD)
        {
            effect_canvas::LedPlans direct;
            for(const auto& sample:direct.Get(zone,0,region))
            {
                if(points.size()>=room_input::MaxInputPoints) break;
                room_input::InputPoint point;
                point.location=zone->controller->GetLocation();point.serial=zone->controller->GetSerial();
                point.name=zone->controller->GetName();point.global_led=zone->start_idx()+sample.led;
                point.keyname=zone->controller->GetLEDName(point.global_led);
                point.u=sample.u;point.v=sample.v;points.push_back(std::move(point));
            }
        }
    }
    for(const auto& event:events)
    {
        const auto age=now-event.time;
        if(age<0||age>0.25) continue;
        const auto device=input_identity.Container(event.device_path);
        if(device.empty()) continue;
        if(!input_deduplication.Accept(device,event.device_path,event.scan,event.time))continue;
        std::vector<std::pair<double,double>> used;
        for(const auto& point:points)
        {
            if(native_taps::PhysicalScan(point.keyname)!=event.scan
               ||input_identity.Container(point.location)!=device) continue;
            const std::pair<double,double> position{point.u,point.v};
            if(std::find(used.begin(),used.end(),position)!=used.end()) continue;
            used.push_back(position);
            tap_history.Add(point.u*320,point.v*200,age,speed);
        }
        if(!used.empty()) ++pending_helper_taps;
    }
}

void SignalFavorite::StepEffect(std::vector<ControllerZone*> zones)
{
    ShaderUniformMap values;
    {
        std::lock_guard<std::mutex> guard(parameters_mutex);
        const auto now = std::chrono::steady_clock::now();
        // Resume does not integrate time spent stopped; UI stalls are capped.
        const double elapsed=clock_running?std::max(0.0,std::chrono::duration<double>(now-previous_tick).count()):0;
        const double dt = std::min(elapsed,0.25);
        previous_tick = now; clock_running = true;
        const std::string speed_key=spec.value("tap_speed_key",std::string());
        const double tap_speed=parameters.contains(speed_key)?parameters.at(speed_key).get<double>():1.0;
        tap_history.Advance(elapsed,tap_speed);
        CollectKeyboardTaps(zones,tap_speed,std::chrono::duration<double>(now.time_since_epoch()).count());
        values=basic_state.Update(spec.at("id"),parameters,dt);
        if(spec.at("id")=="PumpUpBeats")
        {
            const auto frame=pump_state.Update(parameters,dt,CaptureSignalSnapshot(),
                parameters.value("displayLayoutHelper",false)&&(pending_helper_taps%2!=0));
            values["pumpLevels"]={frame.levels,4};
            values["pumpState"]={frame.state,4};
            for(unsigned i=0;i<frame.frequencies.size();++i)
                values["pumpFreq["+std::to_string(i)+"]"].values[0]=frame.frequencies[i];
        }
        pending_helper_taps=0;
        for(const auto& control : spec.at("controls"))
        {
            const std::string key = control.at("key");
            const std::string type = control.at("type");
            const auto& parameter = parameters.at(key);
            ShaderUniform uniform;
            if(type == "color")
            {
                const QColor color(QString::fromStdString(parameter)); uniform.components=3;
                uniform.values = {float(color.redF()),float(color.greenF()),float(color.blueF()),0};
            }
            else if(type == "boolean") uniform.values[0] = parameter.get<bool>() ? 1.0f : 0.0f;
            else if(type == "enum")
            {
                const auto& options = control.at("options");
                uniform.values[0] = float(std::distance(options.begin(),std::find(options.begin(),options.end(),parameter)));
            }
            else
            {
                uniform.values[0] = parameter.get<float>();
                phases[key] += dt * parameter.get<double>();
                values["t_"+key].values[0] = float(phases[key]);
            }
            values["p_"+key] = uniform;
        }
        if(tap[3] != 0) tap[2] += float(dt);
        values["iTap"] = {tap,4};
        const auto& taps=tap_history.Events();
        values["iTapCount"].values[0]=float(taps.size());
        unsigned index=0;
        for(const auto& event:taps)
        {
            const auto suffix="["+std::to_string(index++)+"]";
            values["iTapEvents"+suffix]={{float(event.x),float(event.y),float(event.age),1},4};
            values["iTapMeta"+suffix]={{float(event.seed),float(event.travel),0,0},4};
        }
        if(!taps.empty())
        {
            const auto& event=taps.back();
            values["iTap"]={{float(event.x),float(event.y),float(event.age),1},4};
        }
    }
    Renderer()->UpdateCustomUniforms(values);
    Shaders::StepEffect(std::move(zones));
}

bool SignalFavorite::eventFilter(QObject* object,QEvent* event)
{
    if(object == preview && event->type() == QEvent::MouseButtonPress && !preview->pixmap().isNull())
    {
        const auto size = preview->pixmap().size();
        const auto pos = static_cast<QMouseEvent*>(event)->position();
        const double x = pos.x()-(preview->width()-size.width())/2.0;
        const double y = pos.y()-(preview->height()-size.height())/2.0;
        if(x>=0 && y>=0 && x<size.width() && y<size.height())
        {
            std::lock_guard<std::mutex> guard(parameters_mutex);
            tap = {float(320*x/size.width()),float(200*y/size.height()),0,1};
            tap_history.Add(tap[0],tap[1],0,0);
            ++pending_helper_taps;
            return true;
        }
    }
    return Shaders::eventFilter(object,event);
}

void SignalFavorite::LoadCustomSettings(json settings)
{
    if(!settings.is_object()) return;
    if(settings.contains("parameters") && settings["parameters"].is_object())
    {
        for(auto it=settings["parameters"].begin();it!=settings["parameters"].end();++it) SetParameter(it.key(),it.value());
        SyncEditors();
    }
    // Persist identity/controls, not editable copies of shipped shader code.
    settings.erase("shader_program"); settings.erase("shader_name");
    if(!spec.value("audioReactive",false)) settings["use_audio"]=false;
    else if(!settings.contains("use_audio")||!settings["use_audio"].is_boolean()) settings["use_audio"]=true;
    if(settings.contains("keyboard_reactive")&&settings["keyboard_reactive"].is_boolean())
    {
        { std::lock_guard<std::mutex> guard(parameters_mutex);keyboard_enabled=settings["keyboard_reactive"].get<bool>(); }
        if(keyboard_checkbox) {const QSignalBlocker blocker(keyboard_checkbox);keyboard_checkbox->setChecked(keyboard_enabled);}
        UpdateInputListener();
    }
    Shaders::LoadCustomSettings(settings);
    const auto canvas = Shaders::SaveCustomSettings();
    const QSignalBlocker bw(canvas_width), bh(canvas_height);
    canvas_width->setValue(canvas.at("width")); canvas_height->setValue(canvas.at("height"));
}

json SignalFavorite::SaveCustomSettings()
{
    auto settings = Shaders::SaveCustomSettings();
    settings.erase("shader_program"); settings.erase("shader_name");
    std::lock_guard<std::mutex> guard(parameters_mutex);
    settings["parameters"] = parameters;
    settings["preset"] = spec.at("id"); settings["schema_version"] = 1;
    if(keyboard_checkbox) settings["keyboard_reactive"]=keyboard_enabled;
    return settings;
}
