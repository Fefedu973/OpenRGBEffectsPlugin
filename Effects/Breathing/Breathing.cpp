#include "Breathing.h"
#include "ColorUtils.h"

REGISTER_EFFECT(Breathing);

Breathing::Breathing(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::Breathing)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 200;
    EffectDetails.MinSpeed          = 10;
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom    = true;

    SetSpeed(100);
}

Breathing::~Breathing()
{
    delete ui;
}

void Breathing::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void Breathing::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Fading in and out user selected "
                                         "colors across an entire zone").toStdString();
}

void Breathing::StepEffect(std::vector<ControllerZone*> controller_zones)
{

    Progress += ((Speed / 100.0) / (float)FPS);

    if(Progress >= 3.14159) // PI
    {
        Progress -= 3.14159;

        if(RandomColorsEnabled)
        {
           CurrentColor = ColorUtils::RandomHSVColor();
        }
        else
        {
           rgb2hsv(ui->colorsPicker->Colors().at(colorLoopIndex), &CurrentColor);

           if (colorLoopIndex < (int)(ui->colorsPicker->Colors().size() -1))
           {
               colorLoopIndex++;
           }
           else
           {
               colorLoopIndex = 0;
           }
        }
    }

    CurrentColor.value = pow(sin(Progress),3) * 255;

    for (ControllerZone* controller_zone: controller_zones)
    {
        controller_zone->SetAllZoneLEDs(hsv2rgb(&CurrentColor), Brightness, Temperature, Tint);
    }
}

void Breathing::on_colorsPicker_ColorsChanged()
{
    if (colorLoopIndex > (int)(ui->colorsPicker->Colors().size() -1))
    {
        // The number of colors was reduced so the next color no longer exists so let's restart at 0
        colorLoopIndex = 0;
    }
}

void Breathing::LoadCustomSettings(json settings)
{
    if (settings.contains("colors"))
    {
        ui->colorsPicker->SetColors(settings["colors"]);
    }
}

json Breathing::SaveCustomSettings()
{
    json settings;
    settings["colors"]    = ui->colorsPicker->Colors();
    return settings;
}
