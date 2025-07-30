/*---------------------------------------------------------*\
| PluginInfo.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Info Widget                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "PluginInfo.h"
#include "OpenRGBEffectsPlugin.h"

#include <QDesktopServices>
#include <QUrl>
#include <string>

PluginInfo::PluginInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PluginInfo)
{
    ui->setupUi(this);

    ui->builddate_string->setText(BUILDDATE_STRING);
    ui->version_string->setText(VERSION_STRING);
    ui->git_commit_id->setText(GIT_COMMIT_ID);
    ui->git_commit_date->setText(GIT_COMMIT_DATE);
    ui->git_branch->setText(GIT_BRANCH);
}

PluginInfo::~PluginInfo()
{
    delete ui;
}

void PluginInfo::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void PluginInfo::on_open_plugin_folder_clicked()
{
    filesystem::path config_dir = OpenRGBEffectsPlugin::api->GetConfigurationDirectory() / "plugins";

    QUrl url = QUrl::fromLocalFile(QString::fromStdString(config_dir.string()));

    LOG_VERBOSE("[OpenRGBEffectsPlugin] Opening %s", url.path().toStdString().c_str());

    QDesktopServices::openUrl(url);
}

void PluginInfo::on_download_latest_clicked()
{
    std::string url_string = LATEST_BUILD_URL;
    QUrl url(QString::fromStdString(url_string));
    QDesktopServices::openUrl(url);
}
