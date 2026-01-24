/*---------------------------------------------------------*\
| EffectsName.h                                             |
|                                                           |
|   OpenRGB Effects Plugin Effect Name                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>

struct effect_names
{
    std::string classname;      //Internal Name reference for mapping
    std::string ui_name;        //User friendly name (Untranslated)
};
