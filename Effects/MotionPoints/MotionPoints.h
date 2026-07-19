/*---------------------------------------------------------*\
| MotionPoints.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Motion Points Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_MotionPoints.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

struct MovingPoint
{
    enum Dir {
        LEFT = 0,
        RIGHT = 1,
        UP = 2,
        DOWN = 3
    };

    float x = 0.f;
    float y = 0.f;

    float speed_mult = 1.f;

    Dir dir;

    RGBColor color;

    void Move(float delta){
        switch(dir)
        {
        case LEFT:
        {
            x -= delta;
            break;
        }

        case RIGHT:
        {
            x += delta;
            break;
        }

        case UP:
        {
            y -= delta;
            break;
        }

        case DOWN:
        {
            y += delta;
            break;
        }
        }
    }

    bool IsOut()
    {
        return x < 0.f || y < 0.f || x > 1.f || y > 1.f;
    }

    void Init(RGBColor c)
    {
        dir = static_cast<Dir>(rand() % 4);
        speed_mult = ((double) rand() / (RAND_MAX)) + 1;
        color = c;

        switch(dir)
        {
        case LEFT:
        {
            x = 1.f;
            y = ((double) rand() / (RAND_MAX));
            break;
        }

        case RIGHT:
        {
            x = 0.f;
            y = ((double) rand() / (RAND_MAX));
            break;
        }

        case UP:
        {
            x = ((double) rand() / (RAND_MAX));
            y = 1.f;
            break;
        }

        case DOWN:
        {
            x = ((double) rand() / (RAND_MAX));
            y = 0.f;
            break;
        }
        }
    }

};

namespace Ui {
class MotionPoints;
}

class MotionPoints : public RGBEffect
{
    Q_OBJECT

public:
    explicit MotionPoints(QWidget *parent = nullptr);
    ~MotionPoints();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new MotionPoints;});

    static std::string const ClassName() {return "MotionPoints";}
    static std::string const UI_Name() { return QT_TR_NOOP("Motion Points"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void SetSlider2Val(unsigned int) override;

private:
    Ui::MotionPoints *ui;

    void SetDynamicStrings();

    std::vector<MovingPoint> points;

    void UpdatePoints();

    RGBColor GetColor(float, float, float, float);

private slots:
    void changeEvent(QEvent *event) override;
};
