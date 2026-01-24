/*---------------------------------------------------------*\
| ColorPicker.h                                             |
|                                                           |
|   OpenRGB Effects Plugin Color Picker Widget              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "RGBController.h"
#include "ColorUtils.h"

namespace Ui
{
    class ColorPicker;
}

class ColorPicker : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPicker(QWidget *parent = nullptr);
    ~ColorPicker();

    RGBColor CurrentRGBColor();
    QColor CurrentQColor();

    void SetQColor(QColor);
    void SetRGBColor(RGBColor);

private slots:
    void changeEvent(QEvent *event) override;
    void on_button_clicked();

signals:
    void ColorSelected(QColor);

private:
    Ui::ColorPicker *ui;
    QColor current_color;
};
