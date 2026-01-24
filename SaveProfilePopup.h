/*---------------------------------------------------------*\
| SaveProfilePopup.h                                        |
|                                                           |
|   OpenRGB Effects Plugin Save Profile Popup Widget        |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>

namespace Ui
{
    class SaveProfilePopup;
}

class SaveProfilePopup : public QWidget
{
    Q_OBJECT

public:
    explicit SaveProfilePopup(QWidget *parent = nullptr);
    ~SaveProfilePopup();

    QString Filename();
    bool ShouldLoadAtStartup();
    bool ShouldSaveEffectsState();

    void SetFileName(QString);

signals:
    void Accept();
    void Reject();

private slots:
    void changeEvent(QEvent *event) override;
    void on_save_clicked();
    void on_cancel_clicked();

private:
    Ui::SaveProfilePopup *ui;
};
