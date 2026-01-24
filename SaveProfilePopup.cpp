/*---------------------------------------------------------*\
| SaveProfilePopup.cpp                                      |
|                                                           |
|   OpenRGB Effects Plugin Save Profile Popup Widget        |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include "SaveProfilePopup.h"
#include "ui_SaveProfilePopup.h"
#include "OpenRGBEffectSettings.h"

SaveProfilePopup::SaveProfilePopup(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SaveProfilePopup)
{
    ui->setupUi(this);

    // only letters or numbers, - _ and .
    QRegularExpression re("^[\\w\\-_.]+$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    ui->filename->setValidator(validator);

    std::vector<std::string> filenames = OpenRGBEffectSettings::ListProfiles();

    if(filenames.empty())
    {
        ui->choose_existing_profile->setVisible(false);
        ui->profile_list->setVisible(false);
        ui->create_new_profile->setVisible(false);
    }
    else
    {
        ui->enter_profile_name->setVisible(false);

        for(const std::string& f: filenames)
        {
            ui->profile_list->addItem(QString::fromStdString(f));
        }

        connect(ui->profile_list, &QListWidget::currentItemChanged, [=](){
            ui->filename->setText(ui->profile_list->currentItem()->text());
        });
    }
}

SaveProfilePopup::~SaveProfilePopup()
{
    delete ui;
}

void SaveProfilePopup::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

QString SaveProfilePopup::Filename()
{
    return ui->filename->text().trimmed();
}

bool SaveProfilePopup::ShouldLoadAtStartup()
{
    return ui->load_at_startup->isChecked();
}

bool SaveProfilePopup::ShouldSaveEffectsState()
{
    return ui->save_effects_state->isChecked();
}

void SaveProfilePopup::SetFileName(QString text)
{
    ui->filename->setText(text);
}

void SaveProfilePopup::on_save_clicked()
{
    emit Accept();
}

void SaveProfilePopup::on_cancel_clicked()
{
    emit Reject();
}
