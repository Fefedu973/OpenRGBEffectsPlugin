/*---------------------------------------------------------*\
| EffectList.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Effect List                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include "EffectList.h"
#include "EffectListManager.h"
#include "ui_EffectList.h"
#include "ColorUtils.h"
#include "OpenRGBPluginsFont.h"
#include "OpenRGBEffectSettings.h"

EffectList::EffectList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EffectList)
{
    ui->setupUi(this);

    ui->start_stop_all_button->setFont(OpenRGBPluginsFont::GetFont());
    ui->start_stop_all_button->setText(OpenRGBPluginsFont::icon(OpenRGBPluginsFont::play_track_next_o));
    EnableStartStopButton(false);

    main_menu = new QMenu(ui->new_effect);
    ui->new_effect->setMenu(main_menu);

    unsigned int menu_width = width() - ui->start_stop_all_button->width() - layout()->spacing();
    main_menu->setMinimumWidth(menu_width);


    int scroll_bar_size = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    effect_search = new EffectSearch(ui->new_effect, menu_width - scroll_bar_size);

    QWidgetAction* search_action = new QWidgetAction(this);

    search_action->setDefaultWidget(effect_search);
    main_menu->addAction(search_action);

    connect(effect_search, &EffectSearch::EffectClicked, [=](std::string effect_name){
        main_menu->close();
        AddEffect(effect_name);
    });

    connect(effect_search, &EffectSearch::Searching, [=](bool has_text){
        for(QMenu* menu: sub_menus)
        {
            menu->menuAction()->setVisible(!has_text);
        }
        for(QAction* action: sub_actions)
        {
            action->setVisible(!has_text);
        }
    });

    connect(main_menu, &QMenu::aboutToShow, [=](){
        effect_search->FocusSearch();
    });
}

void EffectList::AddEffectsMenus()
{
    for(auto const& entry: EffectListManager::get()->GetCategorizedEffects())
    {
        std::string category = entry.first;

        QMenu* category_menu = new QMenu(QCoreApplication::translate("RGBEffect", category.c_str()), this);
        main_menu->addMenu(category_menu);

        std::vector<effect_names> effect_names_struct = entry.second;

        std::sort(effect_names_struct.begin(), effect_names_struct.end(), [](effect_names a, effect_names b)
                                                                            {
                                                                                return a.ui_name < b.ui_name;
                                                                            });

        for(effect_names const& effect_names: effect_names_struct)
        {
            QString label = QCoreApplication::translate(effect_names.classname.c_str(), effect_names.ui_name.c_str());
            QAction* effect_action = new QAction(label, this);
            category_menu->addAction(effect_action);

            connect(effect_action, &QAction::triggered, [=](){
                AddEffect(effect_names.classname);
            });

            effect_search->add(effect_names);
        }

        sub_menus.push_back(category_menu);
    }
}

void EffectList::AddEffect(std::string effect_name)
{
    RGBEffect* effect = EffectListManager::get()->GetEffectConstructor(effect_name)();

    effect->SetFPS(OpenRGBEffectSettings::globalSettings.fps);
    effect->SetBrightness(OpenRGBEffectSettings::globalSettings.brightness);
    effect->SetTemperature(OpenRGBEffectSettings::globalSettings.temperature);
    effect->SetTint(OpenRGBEffectSettings::globalSettings.tint);

    // Add random or prefered colors, so we already see something fancy
    std::vector<RGBColor> initial_colors;

    for(unsigned int i = 0; i < effect->EffectDetails.UserColors; i ++)
    {
        if(OpenRGBEffectSettings::globalSettings.use_prefered_colors && i < OpenRGBEffectSettings::globalSettings.prefered_colors.size())
        {
            initial_colors.push_back(OpenRGBEffectSettings::globalSettings.prefered_colors[i]);
        }
        else
        {
            initial_colors.push_back(ColorUtils::RandomRGBColor());
        }
    }

    effect->SetUserColors(initial_colors);
    effect->SetRandomColorsEnabled(OpenRGBEffectSettings::globalSettings.prefer_random);

    emit EffectAdded(effect);
}

EffectList::~EffectList()
{
    delete ui;
}

void EffectList::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void EffectList::on_start_stop_all_button_clicked()
{
    emit ToggleAllEffectsState();
}

void EffectList::EnableStartStopButton(bool state)
{
    ui->start_stop_all_button->setEnabled(state);
}

void EffectList::ShowStartStopButton(bool visible)
{
    ui->start_stop_all_button->setVisible(visible);
}

void EffectList::AddMenu(QMenu* menu)
{
    main_menu->addMenu(menu);
    sub_menus.push_back(menu);
}

void EffectList::AddAction(QAction* action)
{
    main_menu->addAction(action);
    sub_actions.push_back(action);
}

void EffectList::ResetMenus()
{
    main_menu->clear();
    sub_menus.clear();
    sub_actions.clear();

    delete effect_search;
    unsigned int menu_width = width() - ui->start_stop_all_button->width() - layout()->spacing();
    int scroll_bar_size = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    effect_search = new EffectSearch(ui->new_effect, menu_width - scroll_bar_size);
    QWidgetAction* search_action = new QWidgetAction(this);

    search_action->setDefaultWidget(effect_search);
    main_menu->addAction(search_action);
}

