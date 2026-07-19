/*---------------------------------------------------------*\
| ShaderPassEditor.h                                        |
|                                                           |
|   OpenRGB Effects Plugin Shader Pass Editor               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "QGLSLHighlighter.hpp"
#include "QGLSLCompleter.hpp"
#include "ShaderPass.h"

namespace Ui {
class ShaderPassEditor;
}

class ShaderPassEditor : public QWidget
{
    Q_OBJECT

public:
    ShaderPassEditor(QWidget*, ShaderPass*);
    ~ShaderPassEditor();

    void setText(QString);
    void setHighlighter(QGLSLHighlighter*);
    void setCompleter(QGLSLCompleter*);

    QString toPlainText();

    void UpdateStyle(QSyntaxStyle*);

private slots:
    void changeEvent(QEvent *event) override;
    void on_choose_texture_clicked();

private:
    Ui::ShaderPassEditor *ui;
    ShaderPass* pass;
};
