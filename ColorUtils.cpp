/*---------------------------------------------------------*\
| ColorUtils.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Color Utilities                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ColorUtils.h"

hsv_t ColorUtils::RandomHSVColor()
{
    hsv_t hsv;

    hsv.hue = rand() % 360;
    hsv.saturation = 255;
    hsv.value = 255;

    return hsv;
}

RGBColor ColorUtils::FromHue(int hue)
{
    hsv_t hsv;

    hsv.hue = hue;
    hsv.saturation = 255;
    hsv.value = 255;

    return RGBColor(hsv2rgb(&hsv));
}

RGBColor ColorUtils::RandomRGBColor()
{
    hsv_t hsv = RandomHSVColor();
    return RGBColor(hsv2rgb(&hsv));
}

RGBColor ColorUtils::Enlight(RGBColor color, double value)
{
    hsv_t hsv;
    rgb2hsv(color, &hsv);

    hsv.value *= value;

    return RGBColor(hsv2rgb(&hsv));
}

RGBColor ColorUtils::Saturate(RGBColor color, float value)
{
    hsv_t hsv;
    rgb2hsv(color, &hsv);

    hsv.saturation *= value;

    return RGBColor(hsv2rgb(&hsv));
}

RGBColor ColorUtils::Invert(RGBColor color)
{
    int r = (255-RGBGetRValue(color));
    int g = (255-RGBGetGValue(color));
    int b = (255-RGBGetBValue(color));

    return ToRGBColor(r, g, b);
}

RGBColor ColorUtils::Interpolate(RGBColor color1, RGBColor color2, float fraction)
{
    return ToRGBColor(
                InterpolateChannel(RGBGetRValue(color1), RGBGetRValue(color2), fraction),
                InterpolateChannel(RGBGetGValue(color1), RGBGetGValue(color2), fraction),
                InterpolateChannel(RGBGetBValue(color1), RGBGetBValue(color2), fraction)
                );
}

RGBColor ColorUtils::Multiply(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                MultiplyChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                MultiplyChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                MultiplyChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::Screen(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                ScreenChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                ScreenChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                ScreenChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::Overlay(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                OverlayChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                OverlayChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                OverlayChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::Dodge(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                DodgeChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                DodgeChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                DodgeChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::Burn(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                BurnChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                BurnChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                BurnChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::Lighten(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                LightenChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                LightenChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                LightenChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::Darken(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                DarkenChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                DarkenChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                DarkenChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};    

RGBColor ColorUtils::Exclusive(RGBColor color1, RGBColor color2)
{
    return color2 > 0 ? color2 : color1;
};

RGBColor ColorUtils::Difference(RGBColor color1, RGBColor color2)
{
    return ToRGBColor(
                DifferenceChannel(RGBGetRValue(color1), RGBGetRValue(color2)),
                DifferenceChannel(RGBGetGValue(color1), RGBGetGValue(color2)),
                DifferenceChannel(RGBGetBValue(color1), RGBGetBValue(color2))
                );
};

RGBColor ColorUtils::ApplyColorBlendFn(RGBColor c1, RGBColor c2, ColorBlendFn fn)
{
    switch(fn)
    {
        case MULTIPLY:  return Multiply(c1, c2);
        case SCREEN:    return Screen(c1, c2);
        case OVERLAY:   return Overlay(c1, c2);
        case DODGE:     return Dodge(c1, c2);
        case BURN:      return Burn(c1, c2);
        case MASK:      return Mask(c1, c2);
        case LIGHTEN:   return Lighten(c1, c2);
        case DARKEN:    return Darken(c1, c2);
        case EXCLUSIVE: return Exclusive(c1, c2);
        case DIFF:      return Difference(c1, c2);
        default:        return OFF();
    }
}

RGBColor ColorUtils::Mask(RGBColor color1, RGBColor color2)
{
    return color2 > 0 ? color1 : 0;
};

RGBColor ColorUtils::OFF()
{
    return ToRGBColor(0,0,0);
}

RGBColor ColorUtils::fromQColor(QColor c)
{
    return ToRGBColor(c.red(), c.green(), c.blue());
}

QColor ColorUtils::toQColor(RGBColor c)
{
    return QColor(RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c));
}

RGBColor ColorUtils::apply_brightness(RGBColor color, float brightness)
{
    return ToRGBColor(
                (int)( (RGBGetRValue(color)) * brightness),
                (int)( (RGBGetGValue(color)) * brightness),
                (int)( (RGBGetBValue(color)) * brightness)
                );
}

RGBColor ColorUtils::apply_adjustments(RGBColor color, float brightness, int temperature, int tint)
{
    return ToRGBColor(
                (int)( std::clamp<int>(RGBGetRValue(color) * (1.0 + (temperature/255.0)), 0, 255)    * brightness),
                (int)( std::clamp<int>(RGBGetGValue(color) * (1.0 + (tint/255.0)), 0 , 255)          * brightness),
                (int)( std::clamp<int>(RGBGetBValue(color) * (1.0 - (temperature/255.0)), 0 , 255)   * brightness)
                );
}

unsigned char ColorUtils::InterpolateChannel(unsigned char a, unsigned char b, float x)
{
    return (int) ((b - a) * x + a);
}

unsigned char ColorUtils::MultiplyChannel(unsigned char a, unsigned char b)
{
    return b * a / 255;
}

unsigned char ColorUtils::ScreenChannel(unsigned char a, unsigned char b)
{
    return 255 - ((255 - b) * (255 - a) >> 8);;
}

unsigned char ColorUtils::OverlayChannel(unsigned char a, unsigned char b)
{
    return 128 > a ? 2 * b * a / 255 : 255 - 2 * (255 - a) * (255 - b) / 255;
}

unsigned char ColorUtils::DodgeChannel(unsigned char a, unsigned char b)
{
    return 255 == a ? a : std::min<int>(255, (b << 8) / (255 - a));
}

unsigned char ColorUtils::BurnChannel(unsigned char a, unsigned char b)
{
    return 0 == a ? a : std::max<int>(0, 255 - ((255 - b) << 8) / a);
}

unsigned char ColorUtils::MaskChannel(unsigned char a, unsigned char b)
{
    return b > 0 ? a : 0;
}

unsigned char ColorUtils::LightenChannel(unsigned char a, unsigned char b)
{
    return std::max<int>(a, b);
}

unsigned char ColorUtils::DarkenChannel(unsigned char a, unsigned char b)
{
    return std::min<int>(a, b);
}

unsigned char ColorUtils::DifferenceChannel(unsigned char a, unsigned char b)
{
    return std::max<int>(a - b, 0);
}