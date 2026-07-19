/*---------------------------------------------------------*\
| Fill.h                                                    |
|                                                           |
|   OpenRGB Effects Plugin Fill Effect                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Fill.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class Fill;
}

class Fill : public RGBEffect
{
    Q_OBJECT

public:
    explicit Fill(QWidget *parent = nullptr);
    ~Fill();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Fill;});

    static std::string const ClassName() {return "Fill";}
    static std::string const UI_Name() { return QT_TR_NOOP("Fill"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private:
    Ui::Fill *ui;

    void SetDynamicStrings();

    double time = 0;
    double progress = 0;
    double old_progress = 0;
    RGBColor random;

    RGBColor GetColor(unsigned int, unsigned int);

private slots:
    void changeEvent(QEvent *event) override;
};
