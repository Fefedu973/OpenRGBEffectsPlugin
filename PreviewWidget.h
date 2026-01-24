/*---------------------------------------------------------*\
| PreviewWidget.h                                           |
|                                                           |
|   OpenRGB Effects Plugin Preview Widget                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>

class PreviewWidget : public QLabel
{
     Q_OBJECT

public:
    explicit PreviewWidget(QWidget* parent = nullptr): QLabel(parent), original_flags(windowFlags()){};
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void ToggleFullScreen();
    Qt::WindowFlags original_flags;
};
