/*---------------------------------------------------------*\
| AudioSignalProcessor.h                                    |
|                                                           |
|   OpenRGB Effects Plugin Audio Signal Processor           |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSettingsStruct.h"
#include "AudioDataStruct.h"

class AudioSignalProcessor
{
public:
    AudioSignalProcessor();

    void SetNormalization(Audio::AudioSettingsStruct*);
    void Process(int, Audio::AudioSettingsStruct*);
    const Audio::AudioDataStruct& Data();

private:
    Audio::AudioDataStruct  data;
};
