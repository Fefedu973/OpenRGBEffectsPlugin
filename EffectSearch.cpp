/*---------------------------------------------------------*\
| EffectSearch.cpp                                          |
|                                                           |
|   OpenRGB Effects Plugin Effect Search Widget             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QAction>
#include <QScrollBar>
#include "EffectSearch.h"
#include "ui_EffectSearch.h"

EffectSearch::EffectSearch(QWidget *parent,  unsigned int w) :
    QWidget(parent),
    ui(new Ui::EffectSearch)
{
    ui->setupUi(this);
    ui->results->hide();
    ui->results->setMaximumWidth(w);
    ui->results->horizontalScrollBar()->setDisabled(true);
    ui->no_results->hide();
    ui->no_results->setMaximumWidth(w);
}

EffectSearch::~EffectSearch()
{
    delete ui;
}

void EffectSearch::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void EffectSearch::on_search_textChanged(const QString& search)
{
    ui->results->clear();
    ui->results->hide();
    ui->no_results->hide();

    if(search.isEmpty())
    {
        emit Searching(false);
        return;
    }

    bool results = false;

    for(const effect_names& data: effects_names)
    {
        if(QString::fromStdString(data.ui_name).toLower().contains(search.toLower()))
        {
            results = true;
            QString s = QString::fromStdString(data.ui_name);
            ui->results->addItem(s);
        }
    }

    ui->no_results->setVisible(!results);
    ui->results->setVisible(results);
    emit Searching(true);
}

void EffectSearch::add(effect_names data)
{
    effects_names.push_back(data);
}

void EffectSearch::FocusSearch()
{
    ui->search->setFocus();
}

void EffectSearch::on_results_itemClicked(QListWidgetItem* item)
{
    std::string effect_name = item->text().toStdString();

    ui->results->clear();
    ui->results->hide();
    ui->no_results->hide();
    ui->search->clear();

    std::string effect_class_name;

    for(const effect_names& data: effects_names)
    {
        if(effect_name == data.ui_name) {
            effect_class_name = data.classname;
            break;
        }
    }

    if(!effect_class_name.empty())
    {
        emit EffectClicked(effect_class_name);
    }
}

void EffectSearch::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Return)
    {
        if(ui->results->selectedItems().size() == 1)
        {
            emit EffectClicked(ui->results->currentItem()->text().toStdString());
        }
    }
    else if(event->key() == Qt::Key_Down)
    {
        if(!ui->results->hasFocus())
        {
            ui->results->setFocus();
            ui->results->setCurrentRow(0);
        }
    }
    else if(event->key() == Qt::Key_Up)
    {
        if(ui->results->hasFocus() && ui->results->currentRow() == 0)
        {
            ui->search->setFocus();
        }
    }
    else
    {
        if(!ui->search->hasFocus())
        {
            ui->search->setFocus();
        }

        QWidget::keyPressEvent(event);
    }
}
