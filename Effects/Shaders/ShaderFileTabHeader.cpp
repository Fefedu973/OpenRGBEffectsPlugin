/*---------------------------------------------------------*\
| ShaderFileTabHeader.cpp                                   |
|                                                           |
|   OpenRGB Effects Plugin Shader File Tab Header           |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ShaderFileTabHeader.h"
#include "ui_ShaderFileTabHeader.h"

ShaderFileTabHeader::ShaderFileTabHeader(QWidget *parent, std::string name, bool closeable) :
    QWidget(parent),
    ui(new Ui::ShaderFileTabHeader)
{
    ui->setupUi(this);
    ui->name->setText(QString::fromStdString(name));
    ui->close->setVisible(closeable);
}

void ShaderFileTabHeader::on_close_clicked()
{
    emit CloseRequest();
}

ShaderFileTabHeader::~ShaderFileTabHeader()
{
    delete ui;
}

void ShaderFileTabHeader::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}
