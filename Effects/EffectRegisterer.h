/*---------------------------------------------------------*\
| EffectRegisterer.h                                        |
|                                                           |
|   OpenRGB Effects Plugin Effect Registerer                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "EffectListManager.h"

#define EFFECT_REGISTERER(_classname, _ui_name, _category, _constructor)                                \
    static class _register                                                                              \
    {                                                                                                   \
     public:                                                                                            \
       _register()                                                                                      \
       {                                                                                                \
           EffectListManager::get()->RegisterEffect(_classname, _ui_name, _category ,_constructor);     \
        }                                                                                               \
    } _registerer;                                                                                      \

#define REGISTER_EFFECT(T) T::_register T::_registerer;
