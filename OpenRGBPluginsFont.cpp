/*---------------------------------------------------------*\
| OpenRGBPluginsFont.cpp                                    |
|                                                           |
|   OpenRGB Plugins Font                                    |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QFontDatabase>
#include <QList>
#include <QString>
#include "OpenRGBEffectsPlugin.h"
#include "OpenRGBPluginsFont.h"

OpenRGBPluginsFont* OpenRGBPluginsFont::instance;

OpenRGBPluginsFont::OpenRGBPluginsFont(){}

OpenRGBPluginsFont *OpenRGBPluginsFont::Get()
{
    if(!instance)
    {
        instance = new OpenRGBPluginsFont();
        instance->fontId = QFontDatabase::addApplicationFont(":/OpenRGBPlugins.ttf");

        if(instance->fontId == -1)
        {
            LOG_ERROR("Cannot load requested font.");
        }
        else
        {
            QString family = QFontDatabase::applicationFontFamilies(instance->fontId).at(0);
            instance->font = QFont(family, 13);
            instance->font.setStyleStrategy(QFont::PreferAntialias);
        }
    }

    return instance;
}

QString OpenRGBPluginsFont::icon(int glyph)
{
    return QChar(glyph);
}

QFont OpenRGBPluginsFont::GetFont()
{
    return Get()->font;
}
