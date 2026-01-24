/*---------------------------------------------------------*\
| EffectSearch.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Effect Search Widget             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QKeyEvent>
#include "EffectsName.h"

namespace Ui
{
    class EffectSearch;
}

class EffectSearch : public QWidget
{
    Q_OBJECT

public:
    explicit EffectSearch(QWidget *parent = nullptr, unsigned int w = 0);
    ~EffectSearch();

    void add(effect_names);
    void FocusSearch();

private slots:
    void changeEvent(QEvent *event) override;
    void on_search_textChanged(const QString&);
    void on_results_itemClicked(QListWidgetItem*);

signals:
    void EffectClicked(std::string);
    void Searching(bool);


protected:
    void keyPressEvent(QKeyEvent*);

private:
    Ui::EffectSearch *ui;
    std::vector<effect_names> effects_names;
};
