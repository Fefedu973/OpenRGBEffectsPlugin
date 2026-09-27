// SPDX-License-Identifier: GPL-2.0-or-later
#include "SignalFavorite.h"
#include "SignalFavoriteRegistry.h"
#include "EffectListManager.h"
#include "ShaderCanvas.h"
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
                definition.at("title"), "SignalRGB Favorites", [path](){ return new SignalFavorite(path); });
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
    const QString notes = QString::fromStdString(spec.value("notes",std::string()));
    if(!notes.isEmpty()) { auto* label = new QLabel(notes,this); label->setWordWrap(true); outer->addWidget(label); }
    SyncEditors();
    InstallProgram(resource);
    Shaders::LoadCustomSettings({{"width",800},{"height",500}});
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
    std::string prefix = "uniform vec4 iTap;\n";
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
    program->main_pass->data.fragment_shader = prefix + shader.readAll().toStdString();
    program->recompile = true;
}

void SignalFavorite::EffectState(bool enabled)
{
    {
        std::lock_guard<std::mutex> guard(parameters_mutex);
        clock_running = false;
    }
    // Profiles load controls after construction. Prime those values while the
    // renderer is stopped so its first frame cannot flash the factory colors.
    if(enabled && !Renderer()->isRunning()) StepEffect({});
    Shaders::EffectState(enabled);
}

void SignalFavorite::StepEffect(std::vector<ControllerZone*> zones)
{
    ShaderUniformMap values;
    {
        std::lock_guard<std::mutex> guard(parameters_mutex);
        const auto now = std::chrono::steady_clock::now();
        // Resume does not integrate time spent stopped; UI stalls are capped.
        const double dt = clock_running ? std::clamp(std::chrono::duration<double>(now-previous_tick).count(),0.0,0.25) : 0;
        previous_tick = now; clock_running = true;
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
    settings["use_audio"] = false;
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
    return settings;
}
