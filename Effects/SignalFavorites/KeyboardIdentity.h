// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <map>
#include <string>

namespace native_taps
{
class KeyboardIdentity
{
public:
    std::string Container(const std::string& location);
    void Clear() { cache.clear(); }
private:
    struct Entry { std::string value; std::chrono::steady_clock::time_point until; };
    std::map<std::string,Entry> cache;
};
}
