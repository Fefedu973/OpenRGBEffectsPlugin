/*---------------------------------------------------------*\
| Shaders.cpp                                               |
|                                                           |
|   OpenRGB Effects Plugin Shaders Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QDesktopServices>
#include <QInputDialog>
#include <QUrl>
#include <QTimer>
#include <QSignalBlocker>
#include "Audio/AudioManager.h"
#include "OpenRGBEffectSettings.h"
#include "OpenRGBEffectsPlugin.h"
#include "Shaders.h"
#include "ShaderCanvas.h"

REGISTER_EFFECT(Shaders);

Shaders::Shaders(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::Shaders)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 2000;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom    = false;

    shader_renderer = new ShaderRenderer(this);

    editor = new GLSLCodeEditor(this, shader_renderer->Program());

    ui->preview->hide();

    // Connect slots
    connect(editor, &GLSLCodeEditor::Applied, [this](ShaderProgram* program){
        program->Resize(width, height);
        shader_renderer->SetProgram(program);
    });

    connect(shader_renderer, &ShaderRenderer::Image, this, [this](const QImage& image){
        std::lock_guard<std::mutex> guard(image_mutex);
        this->image = image;
        ++image_sequence;
    }, Qt::DirectConnection);

    // A single GUI timer consumes the latest frame. Hidden previews do no work,
    // and a busy GUI cannot accumulate full-resolution pixmaps in its event queue.
    auto* preview_timer = new QTimer(this);
    preview_timer->setInterval(67);
    connect(preview_timer, &QTimer::timeout, this, [this]{
        if(!ui->preview->isVisible()) return;
        QImage latest;
        {
            std::lock_guard<std::mutex> guard(image_mutex);
            if(image_sequence == preview_sequence) return;
            preview_sequence = image_sequence;
            latest = image;
        }
        if(!latest.isNull()) ui->preview->setPixmap(QPixmap::fromImage(latest.scaled(640,360,Qt::KeepAspectRatio,Qt::FastTransformation)));
    });
    preview_timer->start();
    connect(ui->publish_frame, &QCheckBox::toggled, this, [this](bool enabled){
        std::lock_guard<std::mutex> guard(image_mutex); publish_frame = enabled;
    });
    connect(ui->frame_channel, &QLineEdit::textChanged, this, [this](const QString& value){
        std::lock_guard<std::mutex> guard(image_mutex); frame_channel = value.toStdString();
    });
#ifndef SHADERS_HAS_FRAME_SURFACE
    ui->publish_frame->setEnabled(false);
    ui->publish_frame->setToolTip(tr("Requires the Windows OpenRGB Room image-surface headers."));
#endif

    connect(shader_renderer, &ShaderRenderer::Log, editor, &GLSLCodeEditor::SetLog);

    /*-----------------------------------------------*\
    | List the embeded shaders, add them to the       |
    | combo box                                       |
    \*-----------------------------------------------*/
    QDirIterator it(":/shaders", QDir::Files);
    QStringList shader_list;

    while (it.hasNext())
    {
        shader_list << it.next();
    }

    /*-----------------------------------------------*\
    | List the custom shaders paths                   |
    \*-----------------------------------------------*/

    std::vector<std::string> custom_shaders = OpenRGBEffectSettings::ListShaders();

    for(const std::string& shader_path : custom_shaders)
    {
        shader_list << QString::fromStdString(shader_path);
    }

    /*-----------------------------------------------*\
    | Add them to the combo box                       |
    \*-----------------------------------------------*/
    shader_list.sort();

    for(const QString& path: shader_list)
    {
        shader_paths.push_back(path);
        ui->shaders->addItem(path.split( "/" ).last());
    }

    /*--------------------------*\
    | Setup audio                |
    \*--------------------------*/

    memcpy(&audio_settings_struct, &OpenRGBEffectSettings::globalSettings.audio_settings,sizeof(Audio::AudioSettingsStruct));

    audio_signal_processor.SetNormalization(&audio_settings_struct);

    connect(&audio_settings, &AudioSettings::AudioDeviceChanged, this, &Shaders::OnAudioDeviceChanged);
    connect(&audio_settings, &AudioSettings::NormalizationChanged,[=]{
        audio_signal_processor.SetNormalization(&audio_settings_struct);
    });

    audio_settings.SetSettings(&audio_settings_struct);

    SetSpeed(1000);
}

Shaders::~Shaders()
{
    shader_renderer->Stop();
    delete ui;
}

void Shaders::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void Shaders::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Unleash the power of OpenRGB with GL shaders").toStdString();
}

void Shaders::SetFPS(unsigned int value)
{
    FPS = value;
    shader_renderer->SetFPS(value);
}

void Shaders::EffectState(bool state)
{
    EffectEnabled = state;
    image_router.SetRunning(state);
#ifdef SHADERS_HAS_FRAME_SURFACE
    {
        std::lock_guard<std::mutex> guard(publication_mutex);
        publication_running = state;
        if(!state) { frame_publisher.reset(); published_sequence = 0; }
    }
#endif

    if(state)
    {
        shader_renderer->Start();

        if(use_audio)
        {
            StartAudio();
        }
    }
    else
    {
        shader_renderer->Stop();

        if(use_audio)
        {
            StopAudio();
        }
    }
}

void Shaders::StartAudio()
{
    if(audio_settings_struct.audio_device >= 0)
    {
        AudioManager::get()->RegisterClient(audio_settings_struct.audio_device, this);
    }
}

void Shaders::StopAudio()
{
    if(audio_settings_struct.audio_device >= 0)
    {
        AudioManager::get()->UnRegisterClient(audio_settings_struct.audio_device, this);
    }
}

void Shaders::Resize()
{
    shader_renderer->Resize(width, height);
}

void Shaders::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    float new_time = time + 0.001 * Speed / (float) FPS;
    // If we run out of precision and time stands still, reset it
    time = (time == new_time) ? 0 : new_time;

    if(use_audio)
    {
        audio_signal_processor.Process(FPS, &audio_settings_struct);
    }

    if(!shader_renderer->isRunning())
    {
        return;
    }

    shader_renderer->UpdateUniforms(invert_time ? -time : time,
                                   use_audio ? (float*)audio_signal_processor.Data().fft_fltr : nullptr);

    image_mutex.lock();

    if(image.isNull())
    {
        image_mutex.unlock();
        return;
    }

    QImage copy = image;
    const uint64_t sequence = image_sequence;
    const bool publish = publish_frame;
    const std::string channel = frame_channel;
    const auto regions = zone_regions;
    const auto revision = plan_revision;

    image_mutex.unlock();

#ifdef SHADERS_HAS_FRAME_SURFACE
    {
        std::lock_guard<std::mutex> publication_guard(publication_mutex);
        if(publish && publication_running)
        {
            try
            {
                if(!frame_publisher || publisher_channel != channel)
                {
                    frame_publisher.reset();
                    frame_publisher = std::make_unique<room_surface::Publisher>(channel);
                    publisher_channel = channel;
                    published_sequence = 0;
                    if(!frame_publisher->IsOpen()) LOG_WARNING("[Shaders] FrameSurface: %s", frame_publisher->LastError().c_str());
                }
                if(frame_publisher->IsOpen() && (sequence != published_sequence || Brightness != published_brightness ||
                   Temperature != published_temperature || Tint != published_tint))
                {
                    image_router.frame.Update(copy, sequence, Brightness, Temperature, Tint);
                    const auto& output = image_router.frame;
                    if(output.pixels && frame_publisher->PublishBGRA(output.pixels->data(), output.pixels->size(), output.width, output.height, output.stride))
                    {
                        published_sequence=sequence; published_brightness=Brightness;
                        published_temperature=Temperature; published_tint=Tint;
                    }
                }
            }
            catch(const std::exception&) { frame_publisher.reset(); }
        }
        else frame_publisher.reset();
    }
#else
    (void)publish; (void)channel;
#endif

    for(ControllerZone* zone : controller_zones)
    {
        const auto region = effect_canvas::RegionFor(zone, *regions);
        if(image_router.Route(zone,copy,sequence,Brightness,Temperature,Tint,region)) continue;
        const auto& samples = led_plans.Get(zone,revision,region);
        for(const auto& sample : samples)
        {
            const QRgb color = effect_canvas::SamplePixel(copy,sample);
            zone->SetLED(sample.led,ToRGBColor(qRed(color),qGreen(color),qBlue(color)),Brightness,Temperature,Tint);
        }
    }
}

void Shaders::OnControllerZonesListChanged(std::vector<ControllerZone*>)
{
    std::lock_guard<std::mutex> guard(image_mutex);
    ++plan_revision;
}

/*-----------------------------------------------*\
| UI Bindings                                     |
\*-----------------------------------------------*/
void Shaders::on_show_rendering_stateChanged(int state)
{
    ui->preview->setVisible(state);
    show_rendering = state;
}

void Shaders::on_use_audio_stateChanged(int state)
{
    ui->audio_settings->setVisible(state);

    use_audio = state;

    if(use_audio && EffectEnabled)
    {
        StartAudio();
    }

    if(!use_audio && EffectEnabled)
    {
        StopAudio();
    }
}

void Shaders::OnAudioDeviceChanged(int value)
{
    if(!use_audio)
    {
        audio_settings_struct.audio_device = value;
        return;
    }

    bool was_running = EffectEnabled;

    if(EffectEnabled)
    {
        StopAudio();
    }

    audio_settings_struct.audio_device = value;

    if(was_running)
    {
        StartAudio();
    }
}

void Shaders::on_shaders_currentIndexChanged(int idx)
{
    current_shader_idx = idx;

    QFile frag(shader_paths[current_shader_idx]);

    if(frag.open(QFile::ReadOnly | QFile::Text) == false)
    {
        LOG_ERROR("[Shaders] Could not open shader file");
    }

    QTextStream frag_in(&frag);

    ShaderProgram* program = new ShaderProgram();
    program->main_pass->data.fragment_shader = frag_in.readAll().toStdString();
    program->Resize(width, height);
    shader_renderer->SetProgram(program);
    editor->SetProgram(program);
}

void Shaders::on_width_valueChanged(int value)
{
    if(!ShaderCanvas::ValidSize(value,height))
    { QSignalBlocker blocker(ui->width); ui->width->setValue(width); return; }
    width = value;
    Resize();
}

void Shaders::on_height_valueChanged(int value)
{
    if(!ShaderCanvas::ValidSize(width,value))
    { QSignalBlocker blocker(ui->height); ui->height->setValue(height); return; }
    height = value;
    Resize();
}

void Shaders::on_invert_time_stateChanged(int value)
{
    invert_time = value;
}

void Shaders::on_edit_clicked()
{
    editor->show();
}
void Shaders::on_time_reset_clicked()
{
    time = 0.f;
}

void Shaders::on_audio_settings_clicked()
{
    audio_settings.show();
}

void Shaders::on_save_shader_as_clicked()
{
    std::vector<std::string> filenames = OpenRGBEffectSettings::ListShaders();

    QString filename;

    if(filenames.empty())
    {
        filename = QInputDialog::getText(
                    nullptr, tr("Save shader to file..."), tr("Choose a filename"),
                    QLineEdit::Normal, tr("my-shader")).trimmed();
    }
    else
    {
        QDialog dialog;

        dialog.setModal(true);
        dialog.setWindowTitle(tr("Save shader to file..."));

        QLabel text1(tr("Overwrite existing shader:"), &dialog);
        QLabel text2(tr("Or create a new one:"), &dialog);

        QVBoxLayout dialog_layout(&dialog);
        QListWidget list_widget(&dialog);

        for(const std::string& f: filenames)
        {
            QString qf = QString::fromStdString(f);
            list_widget.addItem(qf.split( "/" ).last());
        }

        QLineEdit filename_input(&dialog);

        filename_input.setText(tr("my-shader"));

        dialog_layout.addWidget(&text1);
        dialog_layout.addWidget(&list_widget);
        dialog_layout.addWidget(&text2);
        dialog_layout.addWidget(&filename_input);

        QHBoxLayout buttons_layout;

        QPushButton ok_button;
        ok_button.setText(tr("OK"));
        buttons_layout.addWidget(&ok_button);

        QPushButton cancel_button;
        cancel_button.setText(tr("Cancel"));
        dialog.connect(&cancel_button,SIGNAL(clicked()),&dialog,SLOT(reject()));
        buttons_layout.addWidget(&cancel_button);

        dialog.connect(&ok_button,SIGNAL(clicked()),&dialog,SLOT(accept()));

        dialog_layout.addLayout(&buttons_layout);

        connect(&list_widget, &QListWidget::currentItemChanged, [&](){
            filename_input.setText(list_widget.currentItem()->text());
        });

        if (dialog.exec())
        {
            filename = filename_input.text();
        }
    }

    if(!filename.isEmpty())
    {
        std::string fs = shader_renderer->Program()->main_pass->data.fragment_shader;
        OpenRGBEffectSettings::SaveShader(fs, filename.toStdString());
    }
}

void Shaders::on_open_shaders_folder_clicked()
{
    filesystem::path config_dir = OpenRGBEffectSettings::ShadersFolder();
    QUrl url = QUrl::fromLocalFile(QString::fromStdString(config_dir.string()));
    LOG_VERBOSE("[OpenRGBEffectsPlugin] Opening %s", url.path().toStdString().c_str());
    QDesktopServices::openUrl(url);
}

/*-----------------------------------------------*\
| From/to json                                    |
\*-----------------------------------------------*/
void Shaders::LoadCustomSettings(json Settings)
{
    if(!Settings.is_object()) return;
    if(Settings.contains("publish_frame") && Settings["publish_frame"].is_boolean())
        ui->publish_frame->setChecked(Settings["publish_frame"]);
    if(Settings.contains("frame_channel") && Settings["frame_channel"].is_string())
        ui->frame_channel->setText(QString::fromStdString(Settings["frame_channel"]));
    if(Settings.contains("shader_name"))
        ui->shaders->setCurrentText(QString::fromStdString(Settings["shader_name"]));

    if(Settings.contains("shader_program"))
    {
        shader_renderer->SetProgram(ShaderProgram::FromJSON(Settings["shader_program"]));
        editor->SetProgram(shader_renderer->Program());
    }

    // Validate and install the pair atomically; the previous height must not
    // reject a valid new width during a wide-to-tall profile change.
    try
    {
        const int new_width = Settings.value("width", int(width));
        const int new_height = Settings.value("height", int(height));
        if(new_width > 0 && new_height > 0 && ShaderCanvas::ValidSize(new_width,new_height))
        {
            const QSignalBlocker width_blocker(ui->width), height_blocker(ui->height);
            width = new_width; height = new_height;
            ui->width->setValue(width); ui->height->setValue(height);
            Resize();
        }
    }
    catch(const json::exception&) { LOG_WARNING("[Shaders] Ignoring malformed canvas dimensions"); }
    {
        auto valid = effect_canvas::ValidRegions(Settings.value("zone_regions",json::array()));
        std::lock_guard<std::mutex> guard(image_mutex);
        zone_regions = std::make_shared<const json>(std::move(valid));
        ++plan_revision;
    }

    if(Settings.contains("show_rendering"))
        ui->show_rendering->setChecked(Settings["show_rendering"]);

    if(Settings.contains("invert_time"))
        ui->invert_time->setChecked(Settings["invert_time"]);

    if(Settings.contains("use_audio"))
        ui->use_audio->setChecked(Settings["use_audio"]);

    if (Settings.contains("audio_settings"))
    {
        audio_settings_struct = Settings["audio_settings"];
        audio_settings.SetSettings(&audio_settings_struct);
    }
}

json Shaders::SaveCustomSettings()
{
    json settings;

    settings["shader_name"]      = ui->shaders->currentText().toStdString();
    settings["shader_program"]   = shader_renderer->Program()->ToJSON();
    settings["width"]            = width;
    settings["height"]           = height;
    settings["publish_frame"]    = publish_frame;
    settings["frame_channel"]    = frame_channel;
    { std::lock_guard<std::mutex> guard(image_mutex); settings["zone_regions"] = *zone_regions; }
    settings["show_rendering"]   = show_rendering;
    settings["invert_time"]      = invert_time;
    settings["use_audio"]        = use_audio;
    settings["audio_settings"] = audio_settings_struct;

    return settings;
}
