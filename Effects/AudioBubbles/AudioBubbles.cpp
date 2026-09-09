/*---------------------------------------------------------*\
| AudioBubbles.cpp                                          |
|                                                           |
|   OpenRGB Effects Plugin Audio Bubbles Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "AudioBubbles.h"
#include "ColorUtils.h"
#include "Audio/AudioManager.h"
#include "hsv.h"
#include "OpenRGBEffectSettings.h"
#include <QPainter>

REGISTER_EFFECT(AudioBubbles);

AudioBubbles::AudioBubbles(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::AudioBubbles)
{
    ui->setupUi(this);

    SetDynamicStrings();

    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.SupportsRandom    = false;
    EffectDetails.HasCustomSettings = true;

    gradient = QImage(256, 1, QImage::Format_RGB32);

    /*--------------------------*\
    | Map signal to UpdateGraph  |
    \*--------------------------*/
    connect(this, SIGNAL(UpdateGraphSignal(QPixmap)), this, SLOT(UpdateGraph(QPixmap)));

    /*------------------------------------*\
    | Init preview                         |
    \*------------------------------------*/
    QImage image(256, 100, QImage::Format_RGB30);

    image.fill(Qt::black);

    QPixmap pixmap;
    pixmap.convertFromImage(image);

    ui->preview->setPixmap(pixmap);
    ui->preview->setScaledContents(true);

    /*--------------------------*\
    | Setup audio                |
    \*--------------------------*/
    memcpy(&audio_settings_struct, &OpenRGBEffectSettings::globalSettings.audio_settings,sizeof(Audio::AudioSettingsStruct));

    audio_signal_processor.SetNormalization(&audio_settings_struct);

    connect(&audio_settings, &AudioSettings::AudioDeviceChanged, this, &AudioBubbles::OnAudioDeviceChanged);
    connect(&audio_settings, &AudioSettings::NormalizationChanged,[=]{
        audio_signal_processor.SetNormalization(&audio_settings_struct);
    });

    audio_settings.SetSettings(&audio_settings_struct);

    ui->preset->setCurrentIndex(0);

    GenerateGradient();
}

AudioBubbles::~AudioBubbles()
{
    delete ui;
}

void AudioBubbles::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void AudioBubbles::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Bloop bloop").toStdString();

    ui->preset->clear();

    for(const AudioBubblesPreset& preset: presets)
    {
        ui->preset->addItem(tr(preset.name));
    }

    ui->spawnMode->clear();
    ui->spawnMode->addItems({
        tr("Random XY"),
        tr("Random X"),
        tr("Random Y"),
        tr("Center")
    });
}

void AudioBubbles::EffectState(const bool state)
{
    EffectEnabled = state;
    state ? Start() : Stop();
}

void AudioBubbles::Start()
{
    if(audio_settings_struct.audio_device >= 0)
    {
        AudioManager::get()->RegisterClient(audio_settings_struct.audio_device, this);
    }
}

void AudioBubbles::Stop()
{
    if(audio_settings_struct.audio_device >= 0)
    {
        AudioManager::get()->UnRegisterClient(audio_settings_struct.audio_device, this);
    }
}

void AudioBubbles::on_preset_currentIndexChanged(int value)
{
    if(value > -1)
    {
        ui->colorsPicker->SetColors(presets[value].colors);
    }
}

void AudioBubbles::on_spawnMode_currentIndexChanged(int value)
{
    spawnMode = static_cast<BubblesCenterSpawnMode>(value);
}

void AudioBubbles::OnAudioDeviceChanged(int value)
{
    bool was_running = EffectEnabled;

    if(EffectEnabled)
    {
        Stop();
    }

    audio_settings_struct.audio_device = value;

    if(was_running)
    {
        Start();
    }
}

void AudioBubbles::on_audio_settings_clicked()
{
    audio_settings.show();
}

void AudioBubbles::UpdateGraph(QPixmap pixmap)
{
    if(EffectEnabled)
    {
        ui->preview->setPixmap(pixmap);
        ui->preview->setScaledContents(true);
    }
}

void AudioBubbles::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    audio_signal_processor.Process(FPS, &audio_settings_struct);

    QImage image(256, 100, QImage::Format_RGB30);
    QPixmap pixmap;
    RGBColor white = ToRGBColor(255,255,255);

    for(int i = 0; i < 256; i++)
    {
        QColor color = gradient.pixelColor(i,0);

        float value = audio_signal_processor.Data().fft_fltr[i] * 100;

        for(int y = 0; y < 100; y++)
        {
            if(value > y || y == 0)
            {
                image.setPixelColor(i, 99 - y, color);
            }
            else {
                image.setPixel(i, 99 - y, 0);
            }
        }

        image.setPixel(i, 100 - trigger, white);
    }

    pixmap.convertFromImage(image);
    emit UpdateGraphSignal(pixmap);

    for(ControllerZone* controller_zone : controller_zones)
    {
        zone_type ZT = controller_zone->type();

        if((ZT == ZONE_TYPE_SINGLE)
        || (ZT == ZONE_TYPE_LINEAR)
        || (ZT == ZONE_TYPE_LINEAR_LOOP))
        {            
            int leds_count = controller_zone->leds_count();

            for(int LedID = 0; LedID < leds_count; LedID++)
            {
                controller_zone->SetLED(LedID, GetColor(LedID, 0, leds_count, 1), Brightness, Temperature, Tint);
            }
        }

        else if((ZT == ZONE_TYPE_MATRIX)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_X)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_Y))
        {
            int cols = controller_zone->matrix_map_width();
            int rows = controller_zone->matrix_map_height();

            for(int col_id = 0; col_id < cols; col_id++)
            {
                for(int row_id = 0; row_id < rows; row_id++)
                {
                    RGBColor color = GetColor(col_id, row_id, cols, rows);
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }

    ExpandBubbles();
    TriggerBubbles();
    Cleanup();
}

void AudioBubbles::ExpandBubbles()
{
    for(AudioBubble& bubble: bubbles)
    {
        bubble.progress += 0.1 * speed_mult * bubble.speed / (float) FPS;
    }
}

void AudioBubbles::TriggerBubbles()
{
    float trigger_value = 0.01 * trigger;

    // sort on amplitude
    std::vector<std::pair<float,unsigned int>> indexed_fft;

    for(int i = 0; i < 256; i+= audio_settings_struct.avg_size)
    {
        float amp = audio_signal_processor.Data().fft_fltr[i];

        if(amp >= trigger_value)
        {
            indexed_fft.push_back({audio_signal_processor.Data().fft_fltr[i], i});
        }
    }

    std::sort(indexed_fft.begin(), indexed_fft.end(), [](std::pair<float,unsigned int> &l , std::pair<float,unsigned int> &r){
        return l.first > r.first;
    });

    std::vector<unsigned int> occupied_frequencies;

    for(const AudioBubble& bubble: bubbles)
    {
        occupied_frequencies.push_back(bubble.freq_id);
    }

    for(std::pair<float,unsigned int> p: indexed_fft)
    {
        if(bubbles.size() >= max_bubbles) break;

        if(std::find(occupied_frequencies.begin(), occupied_frequencies.end(), p.second) == occupied_frequencies.end())
        {
            InitBubble(p.second, p.first);
        }
    }
}

void AudioBubbles::InitBubble(int idx, double amp)
{
    AudioBubble bubble;

    bubble.amp = std::clamp<double>(amp, 0.2, 0.8);

    switch (spawnMode)
    {
        case RANDOM_XY:
            bubble.center = QPointF((((double)rand() / RAND_MAX) - 1) *(-1), (((double)rand() / RAND_MAX) - 1) *(-1));
            break;
        case RANDOM_X:
            bubble.center = QPointF((((double)rand() / RAND_MAX) - 1) *(-1), 1.- idx / 256.);
            break;
        case RANDOM_Y:
            bubble.center = QPointF(idx / 256., (((double)rand() / RAND_MAX) - 1) *(-1));
            break;
        case CENTER:
        default:
            bubble.center = QPointF(0.5,0.5);
            break;
    }

    bubble.progress = 0;
    bubble.speed = 1. / bubble.amp;
    bubble.freq_id = idx;

    bubbles.push_back(bubble);
}

void AudioBubbles::Cleanup()
{
    std::vector<AudioBubble> bubbles_to_keep;

    for(const AudioBubble& bubble: bubbles)
    {
        if(bubble.progress < (max_expansion * bubble.amp))
        {
            bubbles_to_keep.push_back(bubble);
        }
    }

    bubbles = bubbles_to_keep;
}

RGBColor AudioBubbles::GetColor(int x, int y, int w, int h)
{
    RGBColor pixel = 0;

    for (const AudioBubble& bubble: bubbles)
    {
        double distance = sqrt(pow(w * bubble.center.x() - x, 2) + pow(h * bubble.center.y() - y, 2));
        double shallow = fabs(distance - bubble.progress) / ( 0.1 * bubbles_thickness * bubble.amp);
        unsigned int value = std::min<double>(255, 255 * (1 / pow(shallow, 3)));
        double progress = std::min<double>(1, bubble.progress / ((max_expansion * bubble.amp)));

        if(value > 0 && progress > 0)
        {
            hsv_t bubble_color_hsv;
            rgb2hsv(ColorUtils::fromQColor(gradient.pixelColor(bubble.freq_id, 0)), &bubble_color_hsv);
            bubble_color_hsv.value = value * pow(1. - progress, 0.5);
            pixel = ColorUtils::Screen(pixel, hsv2rgb(&bubble_color_hsv));
        }
    }

    return pixel;
}

void AudioBubbles::GenerateGradient()
{
    QPointF start_point(0,0);
    QPointF end_point(256,0);

    QLinearGradient grad(start_point, end_point);

    grad.setSpread(QGradient::Spread::PadSpread);

    std::vector<RGBColor> colors = ui->colorsPicker->Colors();
    float step = 1.f / (colors.size() -1);
    float start = 0.f;

    QGradientStops stops;

    for(RGBColor& color: colors)
    {
        QGradientStop stop(start, ColorUtils::toQColor(color));
        start += step;
        stops.append(stop);
    }

    grad.setStops(stops);

    QBrush brush(grad);
    QRectF rect(0, 0, 256, 1);
    QPainter painter(&gradient);

    painter.fillRect(rect, brush);

    ui->gradient->setPixmap(QPixmap::fromImage(gradient));
}

void AudioBubbles::LoadCustomSettings(json settings)
{
    if(settings.contains("trigger"))            ui->max_bubbles->setValue(settings["trigger"]);
    if(settings.contains("max_bubbles"))        ui->max_bubbles->setValue(settings["max_bubbles"]);
    if(settings.contains("speed_mult"))         ui->speed_mult->setValue(settings["speed_mult"]);
    if(settings.contains("max_expansion"))      ui->max_expansion->setValue(settings["max_expansion"]);
    if(settings.contains("bubbles_thickness"))  ui->bubbles_thickness->setValue(settings["bubbles_thickness"]);
    if(settings.contains("colors"))             ui->colorsPicker->SetColors(settings["colors"]);
    if(settings.contains("spawnMode"))          ui->spawnMode->setCurrentIndex(settings["spawnMode"]);

    if (settings.contains("audio_settings"))
    {
        audio_settings_struct = settings["audio_settings"];
        audio_settings.SetSettings(&audio_settings_struct);
    }
}

json AudioBubbles::SaveCustomSettings()
{
    json settings;

    settings["colors"]              = ui->colorsPicker->Colors();
    settings["trigger"]             = trigger;
    settings["max_bubbles"]         = max_bubbles;
    settings["speed_mult"]          = speed_mult;
    settings["max_expansion"]       = max_expansion;
    settings["bubbles_thickness"]   = bubbles_thickness;
    settings["audio_settings"]      = audio_settings_struct;
    settings["spawnMode"]           = spawnMode;

    return settings;
}

void AudioBubbles::on_colorsPicker_ColorsChanged()
{
    GenerateGradient();
}

void AudioBubbles::on_trigger_valueChanged(int value)
{
    trigger = value;
}
void AudioBubbles::on_max_bubbles_valueChanged(int value)
{
    max_bubbles = value;
}

void AudioBubbles::on_max_expansion_valueChanged(int value)
{
    max_expansion = value;
}

void AudioBubbles::on_bubbles_thickness_valueChanged(int value)
{
    bubbles_thickness = value;
}

void AudioBubbles::on_speed_mult_valueChanged(int value)
{
    speed_mult = value;
}
