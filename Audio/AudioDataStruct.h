/*---------------------------------------------------------*\
| AudioDataStruct.h                                         |
|                                                           |
|   OpenRGB Effects Plugin Audio Data Structures            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

namespace Audio
{
    struct AudioDataStruct
    {
        unsigned char buffer[256];

        float         fft[256];
        float         fft_nrml[256];
        float         fft_fltr[256] = { 0 };

        float         win_hanning[256];
        float         win_hamming[256];
        float         win_blackman[256];
};
}
