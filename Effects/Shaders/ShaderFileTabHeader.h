/*---------------------------------------------------------*\
| ShaderFileTabHeader.h                                     |
|                                                           |
|   OpenRGB Effects Plugin Shader File Tab Header           |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>

namespace Ui {
class ShaderFileTabHeader;
}

class ShaderFileTabHeader : public QWidget
{
    Q_OBJECT

public:
    ShaderFileTabHeader(QWidget*, std::string, bool);
    ~ShaderFileTabHeader();

private slots:
    void changeEvent(QEvent *event) override;
    void on_close_clicked();

private:
    Ui::ShaderFileTabHeader *ui;

signals:
    void CloseRequest();
};
