/*---------------------------------------------------------*\
| ColorUtils.h                                              |
|                                                           |
|   OpenRGB Effects Plugin Color Utilities                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QColor>
#include "hsv.h"
#include "RGBControllerInterface.h"

#define HEXCOLOR(rgb) (ToRGBColor(RGBGetBValue(rgb), RGBGetGValue(rgb), RGBGetRValue(rgb)))

enum ColorBlendFn
{
    MULTIPLY    = 0,
    SCREEN      = 1,
    OVERLAY     = 2,
    DODGE       = 3,
    BURN        = 4,
    MASK        = 5,
    LIGHTEN     = 6,
    DARKEN      = 7,
    EXCLUSIVE   = 8,
    DIFF        = 9
};

static std::vector<std::string> COLOR_BLEND_FN_NAMES =
{
    QT_TRANSLATE_NOOP("ColorUtils", "Multiply"),
    QT_TRANSLATE_NOOP("ColorUtils", "Screen"),
    QT_TRANSLATE_NOOP("ColorUtils", "Overlay"),
    QT_TRANSLATE_NOOP("ColorUtils", "Dodge"),
    QT_TRANSLATE_NOOP("ColorUtils", "Burn"),
    QT_TRANSLATE_NOOP("ColorUtils", "Mask"),
    QT_TRANSLATE_NOOP("ColorUtils", "Lighten"),
    QT_TRANSLATE_NOOP("ColorUtils", "Darken"),
    QT_TRANSLATE_NOOP("ColorUtils", "Exclusive"),
    QT_TRANSLATE_NOOP("ColorUtils", "Difference")
};

class ColorUtils
{
public:
    static hsv_t            RandomHSVColor();
    static RGBColor         FromHue(int hue);
    static RGBColor         RandomRGBColor();
    static RGBColor         Enlight(RGBColor color, double value);
    static RGBColor         Saturate(RGBColor color, float value);
    static RGBColor         Invert(RGBColor color);
    static RGBColor         Interpolate(RGBColor color1, RGBColor color2, float fraction);
    static RGBColor         Multiply(RGBColor color1, RGBColor color2);
    static RGBColor         Screen(RGBColor color1, RGBColor color2);
    static RGBColor         Overlay(RGBColor color1, RGBColor color2);
    static RGBColor         Dodge(RGBColor color1, RGBColor color2);
    static RGBColor         Burn(RGBColor color1, RGBColor color2);
    static RGBColor         Lighten(RGBColor color1, RGBColor color2);
    static RGBColor         Darken(RGBColor color1, RGBColor color2);
    static RGBColor         Exclusive(RGBColor color1, RGBColor color2);
    static RGBColor         Difference(RGBColor color1, RGBColor color2);
    static RGBColor         ApplyColorBlendFn(RGBColor c1, RGBColor c2, ColorBlendFn fn);
    static RGBColor         Mask(RGBColor color1, RGBColor color2);
    static RGBColor         OFF();
    static RGBColor         fromQColor(QColor c);
    static QColor           toQColor(RGBColor c);
    static RGBColor         apply_brightness(RGBColor color, float brightness);
    static RGBColor         apply_adjustments(RGBColor color, float brightness, int temperature, int tint);

private:
    static unsigned char    InterpolateChannel(unsigned char a, unsigned char b, float x);
    static unsigned char    MultiplyChannel(unsigned char a, unsigned char b);
    static unsigned char    ScreenChannel(unsigned char a, unsigned char b);
    static unsigned char    OverlayChannel(unsigned char a, unsigned char b);
    static unsigned char    DodgeChannel(unsigned char a, unsigned char b);
    static unsigned char    BurnChannel(unsigned char a, unsigned char b);
    static unsigned char    MaskChannel(unsigned char a, unsigned char b);
    static unsigned char    LightenChannel(unsigned char a, unsigned char b);
    static unsigned char    DarkenChannel(unsigned char a, unsigned char b);
    static unsigned char    DifferenceChannel(unsigned char a, unsigned char b);

};
