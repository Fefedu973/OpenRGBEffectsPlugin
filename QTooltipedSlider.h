/*---------------------------------------------------------*\
| QTooltipedSlider.h                                        |
|                                                           |
|   Qt Slider with value tooltip widget                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QSlider>

class QTooltipedSlider : public QSlider
{
    Q_OBJECT

public:
    explicit QTooltipedSlider(QWidget *parent = nullptr);
};
