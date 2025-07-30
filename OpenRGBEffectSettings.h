/*---------------------------------------------------------*\
| OpenRGBEffectSettings.h                                   |
|                                                           |
|   OpenRGB Effects Plugin Settings                         |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include "AudioSettingsStruct.h"
#include "filesystem.h"
#include "RGBControllerInterface.h"

struct GlobalSettingsStruct
{
    int  fpscapture = 60;
    int  brightness = 100;
    int  fps = 60;
    int  temperature = 0;
    int  tint = 0;
    bool hide_unsupported = true;
    bool prefer_random = false;
    bool use_prefered_colors = false;
    int  startup_timeout = 2000;
    std::vector<RGBColor> prefered_colors;
    Audio::AudioSettingsStruct      audio_settings;

};

using json = nlohmann::json;

class OpenRGBEffectSettings
{

public:    
    static unsigned int version;

    static bool WriteGlobalSettings();
    static void LoadGlobalSettings();

    static bool SaveEffectPattern(json, std::string, std::string);
    static std::vector<std::string> ListPattern(std::string);
    static json LoadPattern(std::string, std::string);

    static filesystem::path PatternsFolder();

    static filesystem::path ShadersFolder();
    static std::vector<std::string> ListShaders();
    static bool SaveShader(std::string, std::string);

    static GlobalSettingsStruct globalSettings;

private:
    static bool CreateSettingsDirectory();
    static bool CreateEffectPatternsDirectory(std::string);
    static bool CreateShadersDirectory();

    static filesystem::path SettingsFolder();

    static bool create_dir(filesystem::path);
    static std::vector<std::string> list_files(filesystem::path, bool full_path = false);
    static json load_json_file(filesystem::path);
    static bool write_json_to_file(filesystem::path, json);
    static bool write_text_to_file(filesystem::path, std::string);

};
