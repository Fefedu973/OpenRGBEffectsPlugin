/*---------------------------------------------------------*\
| NewShaderPassTabHeader.h                                  |
|                                                           |
|   OpenRGB Effects Plugin New Shader Pass Tab Header       |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ShaderPass.h"

namespace Ui {
class NewShaderPassTabHeader;
}

class NewShaderPassTabHeader : public QWidget
{
    Q_OBJECT

public:
    explicit NewShaderPassTabHeader(QWidget *parent = nullptr);
    ~NewShaderPassTabHeader();

private slots:
    void changeEvent(QEvent *event) override;
    void on_add_clicked();

private:
    Ui::NewShaderPassTabHeader *ui;

    void SetDynamicStrings();

signals:
    void Added(ShaderPass::Type);
};
