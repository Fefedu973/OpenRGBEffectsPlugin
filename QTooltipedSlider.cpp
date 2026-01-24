/*---------------------------------------------------------*\
| QTooltipedSlider.cpp                                      |
|                                                           |
|   Qt Slider with value tooltip widget                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QToolTip>
#include "QTooltipedSlider.h"

QTooltipedSlider::QTooltipedSlider(QWidget *parent) :
    QSlider(parent)
{    
    connect(this, &QSlider::sliderMoved,[&](int value) {
        QToolTip::showText(QCursor::pos(), QString("%1").arg(value), nullptr);
    });
}
