/*---------------------------------------------------------*\
| PluginInfo.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Info Widget                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_PluginInfo.h"

namespace Ui
{
    class PluginInfo;
}

class PluginInfo : public QWidget
{
    Q_OBJECT

public:
    explicit PluginInfo(QWidget *parent = nullptr);
    ~PluginInfo();

private slots:
    void changeEvent(QEvent *event) override;
    void on_open_plugin_folder_clicked();
    void on_download_latest_clicked();

private:
    Ui::PluginInfo *ui;
};
