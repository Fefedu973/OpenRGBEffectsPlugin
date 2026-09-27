/*---------------------------------------------------------*\
| ShaderPassData.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Shader Pass Data                 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>

struct ShaderPassData
{
    std::string fragment_shader;
    std::string texture_path;
    bool feedback = false;
    // Zero follows the effect canvas. Bounded fixed-size intermediate passes
    // allow reductions/filters without forcing every pass to output size.
    int width = 0, height = 0;
    unsigned image_slot = 0;
};
