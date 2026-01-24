/*---------------------------------------------------------*\
| ColorPicker.cpp                                           |
|                                                           |
|   OpenRGB Effects Plugin Color Picker Widget              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QString>
#include <QFile>
#include <QDialog>
#include <QVBoxLayout>
#include <QColorDialog>
#include "ColorUtils.h"
#include "ColorPicker.h"
#include "ui_ColorPicker.h"

ColorPicker::ColorPicker(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ColorPicker)
{
    ui->setupUi(this);
    ui->button->setStyleSheet("QPushButton {background-color: black; border: 1px solid black;}");
}

ColorPicker::~ColorPicker()
{
    delete ui;
}

void ColorPicker::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void ColorPicker::SetQColor(QColor color)
{
    current_color = color;
    ui->button->setStyleSheet("QPushButton {background-color: "+ color.name() + "; border: 1px solid black;}");
}

void ColorPicker::SetRGBColor(RGBColor color)
{
    SetQColor(ColorUtils::toQColor(color));
}

void ColorPicker::on_button_clicked()
{
    QColorDialog *colorDialog = new QColorDialog(this);
    colorDialog->setAttribute(Qt::WA_DeleteOnClose);
    colorDialog->setCurrentColor(current_color);

    connect(colorDialog, &QColorDialog::colorSelected,[=](const QColor &color){
               SetQColor(color);
               emit ColorSelected(color);
    });

    colorDialog->open();
}

QColor ColorPicker::CurrentQColor()
{
    return current_color;
}

RGBColor ColorPicker::CurrentRGBColor()
{
    return ColorUtils::fromQColor(current_color);
}
