#ifndef AMBIENT_H
#define AMBIENT_H

#include <mutex>
#include <QWidget>
#include <QMouseEvent>
#include "ui_Ambient.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "RectangleSelector.h"
#include "ScreenCapturer.h"

namespace Ui {
class Ambient;
}

enum AmbientMode{
    SCALED_AVERAGE = 0,
    SCREEN_COPY = 1
};

class Ambient : public RGBEffect
{
    Q_OBJECT

public:
    explicit Ambient(QWidget *parent = nullptr);
    ~Ambient();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Ambient;});

    static std::string const ClassName() {return "Ambient";}
    static std::string const UI_Name() { return QT_TR_NOOP("Ambient"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void EffectState(bool) override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_select_screen_clicked();
    void on_select_rectangle_clicked();
    void on_left_valueChanged(int);
    void on_top_valueChanged(int);
    void on_width_valueChanged(int);
    void on_height_valueChanged(int);
    void on_mode_currentIndexChanged(int);
    void on_screen_currentIndexChanged(int);
    void on_smoothness_valueChanged(int);
    void on_framerate_valueChanged(int);
    void on_crop_stream_stateChanged(int);

private:
    Ui::Ambient *ui;
    ScreenCapturer* capturer = nullptr;
    RectangleSelectorOverlay* rectangle_selector_overlay = nullptr;

    int screen_index = -1;
    AmbientMode mode = SCALED_AVERAGE;

    void UpdateSelection();
    void SetDynamicStrings();
    RGBColor Smooth(const RGBColor& previous_color, RGBColor color);

    unsigned int left = 0;
    unsigned int top = 0;
    unsigned int width = 1;
    unsigned int height = 1;
    unsigned int smoothness = 80;
    unsigned int framerate = 60;
    bool crop_stream = false;
    QString restore_token;

    QImage image;
    std::mutex lock;
};

#endif // AMBIENT_H
