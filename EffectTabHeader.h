/*---------------------------------------------------------*\
| EffectTabHeader.h                                         |
|                                                           |
|   OpenRGB Effects Plugin Effect Tab Header Widget         |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "RGBEffect.h"

namespace Ui
{
    class EffectTabHeader;
}

class EffectTabHeader : public QWidget
{
    Q_OBJECT

public:
    explicit EffectTabHeader(QWidget *parent = nullptr, RGBEffect* effect = nullptr);
    ~EffectTabHeader();

    void ToogleRunningIndicator(bool);

signals:
    void CloseRequest();
    void Renamed(std::string);
    void StartStopRequest();

private slots:
    void changeEvent(QEvent *event) override;
    void on_close_clicked();
    void on_rename_clicked();
    void on_start_stop_clicked();

private:
    Ui::EffectTabHeader *ui;
    RGBEffect           *effect;
};
