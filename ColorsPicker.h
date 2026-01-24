/*---------------------------------------------------------*\
| ColorsPicker.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Colors Picker Widget             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ColorPicker.h"

namespace Ui
{
    class ColorsPicker;
}

class ColorsPicker : public QWidget
{
    Q_OBJECT

public:
    explicit ColorsPicker(QWidget *parent = nullptr);
    ~ColorsPicker();

    std::vector<RGBColor> Colors();
    void SetColors(std::vector<RGBColor>);
    void SetText(std::string);

signals:
    void ColorsChanged();

private slots:
    void changeEvent(QEvent *event) override;
    void on_colors_count_spinBox_valueChanged(int);

private:
    Ui::ColorsPicker *ui;

    void ResetColors();
    ColorPicker* CreatePicker(int);
    std::vector<RGBColor> colors;
    std::vector<ColorPicker*> color_pickers;

};
