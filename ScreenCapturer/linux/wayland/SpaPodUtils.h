/*---------------------------------------------------------*\
| SpaPodUtils.h                                             |
|                                                           |
|   OpenRGB Effects Plugin SPA Pod Utilities                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <spa/pod/builder.h>

class SpaPodUtils
{
public:
    static spa_pod* CreateFormatOptions(spa_pod_builder* builder, const struct spa_rectangle* resolution, unsigned int framerate);
};
