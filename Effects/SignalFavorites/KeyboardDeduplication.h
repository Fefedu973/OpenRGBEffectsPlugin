// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <map>
#include <string>
#include <cstdint>
namespace native_taps
{
// Composite keyboards can report one physical make through two HID collections.
// Merge only near-simultaneous copies from DIFFERENT collections, not successive
// presses on the same collection. The tiny transient cache is never persisted.
class KeyboardDeduplication
{
    struct Seen { std::string path; double time; };
    std::map<std::pair<std::string,std::uint16_t>,Seen> recent;
public:
    void Clear(){recent.clear();}
    bool Accept(const std::string& container,const std::string& path,std::uint16_t scan,double time)
    {
        for(auto it=recent.begin();it!=recent.end();)
            if(time-it->second.time>0.01)it=recent.erase(it);else++it;
        const auto key=std::make_pair(container,scan);
        const auto prior=recent.find(key);
        if(prior!=recent.end()&&prior->second.path!=path&&time>=prior->second.time&&time-prior->second.time<=0.01)return false;
        if(recent.size()>=128)recent.clear();
        recent[key]={path,time};return true;
    }
};
}
