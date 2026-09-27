// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace native_taps
{
// Standard OpenRGB EN names denote physical positions, independent of the OS
// character layout. Do not turn typed text or VK codes into these identities.
inline std::uint16_t PhysicalScan(const std::string& name)
{
    static const std::map<std::string,std::uint16_t> scans={
        {"Escape",0x01},{"1",0x02},{"2",0x03},{"3",0x04},{"4",0x05},{"5",0x06},
        {"6",0x07},{"7",0x08},{"8",0x09},{"9",0x0a},{"0",0x0b},{"-",0x0c},{"=",0x0d},{"+",0x0d},
        {"Backspace",0x0e},{"Tab",0x0f},{"Q",0x10},{"W",0x11},{"E",0x12},{"R",0x13},{"T",0x14},
        {"Y",0x15},{"U",0x16},{"I",0x17},{"O",0x18},{"P",0x19},{"[",0x1a},{"]",0x1b},
        {"Enter",0x1c},{"Enter (ISO)",0x1c},{"Left Control",0x1d},
        {"A",0x1e},{"S",0x1f},{"D",0x20},{"F",0x21},{"G",0x22},{"H",0x23},{"J",0x24},{"K",0x25},
        {"L",0x26},{";",0x27},{"'",0x28},{"`",0x29},{"Left Shift",0x2a},
        {"\\",0x2b},{"\\ (ANSI)",0x2b},{"#",0x2b},{"Z",0x2c},{"X",0x2d},{"C",0x2e},
        {"V",0x2f},{"B",0x30},{"N",0x31},{"M",0x32},{",",0x33},{".",0x34},{"/",0x35},
        {"Right Shift",0x36},{"Number Pad *",0x37},{"Left Alt",0x38},{"Space",0x39},{"Caps Lock",0x3a},
        {"F1",0x3b},{"F2",0x3c},{"F3",0x3d},{"F4",0x3e},{"F5",0x3f},{"F6",0x40},
        {"F7",0x41},{"F8",0x42},{"F9",0x43},{"F10",0x44},{"Num Lock",0x45},{"Scroll Lock",0x46},
        {"Number Pad 7",0x47},{"Number Pad 8",0x48},{"Number Pad 9",0x49},{"Number Pad -",0x4a},
        {"Number Pad 4",0x4b},{"Number Pad 5",0x4c},{"Number Pad 6",0x4d},{"Number Pad +",0x4e},
        {"Number Pad 1",0x4f},{"Number Pad 2",0x50},{"Number Pad 3",0x51},
        {"Number Pad 0",0x52},{"Number Pad .",0x53},{"\\ (ISO)",0x56},{"F11",0x57},{"F12",0x58},
        {"Number Pad Enter",0xe01c},{"Right Control",0xe01d},{"Number Pad /",0xe035},
        {"Print Screen",0xe037},{"Right Alt",0xe038},{"Home",0xe047},{"Up Arrow",0xe048},
        {"Page Up",0xe049},{"Left Arrow",0xe04b},{"Right Arrow",0xe04d},{"End",0xe04f},
        {"Down Arrow",0xe050},{"Page Down",0xe051},{"Insert",0xe052},{"Delete",0xe053},
        {"Left Windows",0xe05b},{"Right Windows",0xe05c},{"Menu",0xe05d},{"Pause/Break",0xe11d}
    };
    if(name.rfind("Key: ",0)!=0) return 0;
    const auto found=scans.find(name.substr(5));
    return found==scans.end()?0:found->second;
}
}
