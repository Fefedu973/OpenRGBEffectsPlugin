/*---------------------------------------------------------*\
| SpaPodUtils.cpp                                           |
|                                                           |
|   OpenRGB Effects Plugin SPA Pod Utilities                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "SpaPodUtils.h"

#include <spa/param/video/format-utils.h>

spa_pod* SpaPodUtils::CreateFormatOptions(spa_pod_builder* builder, const struct spa_rectangle* resolution, unsigned int framerate) {

    spa_pod_frame frames[2];
    spa_pod_builder_push_object(builder, &frames[0], SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
    spa_pod_builder_add(builder, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
    spa_pod_builder_add(builder, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_format,
                        SPA_POD_CHOICE_ENUM_Id(78,
                                               SPA_VIDEO_FORMAT_AYUV,
                                               SPA_VIDEO_FORMAT_RGBx,
                                               SPA_VIDEO_FORMAT_BGRx,
                                               SPA_VIDEO_FORMAT_xRGB,
                                               SPA_VIDEO_FORMAT_xBGR,
                                               SPA_VIDEO_FORMAT_RGBA,
                                               SPA_VIDEO_FORMAT_BGRA,
                                               SPA_VIDEO_FORMAT_ARGB,
                                               SPA_VIDEO_FORMAT_ABGR,
                                               SPA_VIDEO_FORMAT_RGB,
                                               SPA_VIDEO_FORMAT_BGR,
                                               SPA_VIDEO_FORMAT_Y41B,
                                               SPA_VIDEO_FORMAT_Y42B,
                                               SPA_VIDEO_FORMAT_YVYU,
                                               SPA_VIDEO_FORMAT_Y444,
                                               SPA_VIDEO_FORMAT_v210,
                                               SPA_VIDEO_FORMAT_v216,
                                               SPA_VIDEO_FORMAT_NV12,
                                               SPA_VIDEO_FORMAT_NV21,
                                               SPA_VIDEO_FORMAT_GRAY8,
                                               SPA_VIDEO_FORMAT_GRAY16_BE,
                                               SPA_VIDEO_FORMAT_GRAY16_LE,
                                               SPA_VIDEO_FORMAT_v308,
                                               SPA_VIDEO_FORMAT_RGB16,
                                               SPA_VIDEO_FORMAT_BGR16,
                                               SPA_VIDEO_FORMAT_RGB15,
                                               SPA_VIDEO_FORMAT_BGR15,
                                               SPA_VIDEO_FORMAT_UYVP,
                                               SPA_VIDEO_FORMAT_A420,
                                               SPA_VIDEO_FORMAT_RGB8P,
                                               SPA_VIDEO_FORMAT_YUV9,
                                               SPA_VIDEO_FORMAT_YVU9,
                                               SPA_VIDEO_FORMAT_IYU1,
                                               SPA_VIDEO_FORMAT_ARGB64,
                                               SPA_VIDEO_FORMAT_AYUV64,
                                               SPA_VIDEO_FORMAT_r210,
                                               SPA_VIDEO_FORMAT_I420_10BE,
                                               SPA_VIDEO_FORMAT_I420_10LE,
                                               SPA_VIDEO_FORMAT_I422_10BE,
                                               SPA_VIDEO_FORMAT_I422_10LE,
                                               SPA_VIDEO_FORMAT_Y444_10BE,
                                               SPA_VIDEO_FORMAT_Y444_10LE,
                                               SPA_VIDEO_FORMAT_GBR,
                                               SPA_VIDEO_FORMAT_GBR_10BE,
                                               SPA_VIDEO_FORMAT_GBR_10LE,
                                               SPA_VIDEO_FORMAT_NV16,
                                               SPA_VIDEO_FORMAT_NV24,
                                               SPA_VIDEO_FORMAT_NV12_64Z32,
                                               SPA_VIDEO_FORMAT_A420_10BE,
                                               SPA_VIDEO_FORMAT_A420_10LE,
                                               SPA_VIDEO_FORMAT_A422_10BE,
                                               SPA_VIDEO_FORMAT_A422_10LE,
                                               SPA_VIDEO_FORMAT_A444_10BE,
                                               SPA_VIDEO_FORMAT_A444_10LE,
                                               SPA_VIDEO_FORMAT_NV61,
                                               SPA_VIDEO_FORMAT_P010_10BE,
                                               SPA_VIDEO_FORMAT_P010_10LE,
                                               SPA_VIDEO_FORMAT_IYU2,
                                               SPA_VIDEO_FORMAT_VYUY,
                                               SPA_VIDEO_FORMAT_I420,
                                               SPA_VIDEO_FORMAT_YV12,
                                               SPA_VIDEO_FORMAT_YUY2,
                                               SPA_VIDEO_FORMAT_UYVY,
                                               SPA_VIDEO_FORMAT_GBRA,
                                               SPA_VIDEO_FORMAT_GBRA_10BE,
                                               SPA_VIDEO_FORMAT_GBRA_10LE,
                                               SPA_VIDEO_FORMAT_GBR_12BE,
                                               SPA_VIDEO_FORMAT_GBR_12LE,
                                               SPA_VIDEO_FORMAT_GBRA_12BE,
                                               SPA_VIDEO_FORMAT_GBRA_12LE,
                                               SPA_VIDEO_FORMAT_I420_12BE,
                                               SPA_VIDEO_FORMAT_I420_12LE,
                                               SPA_VIDEO_FORMAT_I422_12BE,
                                               SPA_VIDEO_FORMAT_I422_12LE,
                                               SPA_VIDEO_FORMAT_Y444_12BE,
                                               SPA_VIDEO_FORMAT_Y444_12LE,
                                               SPA_VIDEO_FORMAT_RGBA_F16,
                                               SPA_VIDEO_FORMAT_RGBA_F32
                                               ), 0);

    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(resolution), 0);

    spa_fraction pwFramerateMin = SPA_FRACTION(0, 1);
    spa_fraction pwFramerateMax = SPA_FRACTION(framerate, 1);

    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&pwFramerateMin),0);
    spa_pod_builder_add(builder, SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(&pwFramerateMax, &pwFramerateMin, &pwFramerateMax),0);


    return static_cast<spa_pod*>(spa_pod_builder_pop(builder, &frames[0]));
}
