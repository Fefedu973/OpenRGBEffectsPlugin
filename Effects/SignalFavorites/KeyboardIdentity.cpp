// SPDX-License-Identifier: GPL-2.0-or-later
#include "KeyboardIdentity.h"
#include <QString>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <vector>
#endif

namespace native_taps
{
static std::string Resolve(const std::string& location)
{
#ifdef _WIN32
    // Different HID collections of one keyboard have different interface paths
    // but share Windows' physical-device container. VID/PID alone is ambiguous.
    const auto offset=location.find("\\\\?\\");
    if(offset==std::string::npos) return {};
    const auto path=QString::fromStdString(location.substr(offset)).toStdWString();
    DEVPROPTYPE type=0; ULONG bytes=0;
    if(CM_Get_Device_Interface_PropertyW(path.c_str(),&DEVPKEY_Device_InstanceId,&type,nullptr,&bytes,0)!=CR_BUFFER_SMALL
       ||!bytes||bytes>65536) return {};
    std::vector<wchar_t> instance((bytes+sizeof(wchar_t)-1)/sizeof(wchar_t)+1,0);
    if(CM_Get_Device_Interface_PropertyW(path.c_str(),&DEVPKEY_Device_InstanceId,&type,
          reinterpret_cast<PBYTE>(instance.data()),&bytes,0)!=CR_SUCCESS||type!=DEVPROP_TYPE_STRING) return {};
    DEVINST node=0;
    if(CM_Locate_DevNodeW(&node,instance.data(),CM_LOCATE_DEVNODE_NORMAL)!=CR_SUCCESS) return {};
    GUID container{}; bytes=sizeof(container);
    if(CM_Get_DevNode_PropertyW(node,&DEVPKEY_Device_ContainerId,&type,
          reinterpret_cast<PBYTE>(&container),&bytes,0)!=CR_SUCCESS||type!=DEVPROP_TYPE_GUID||bytes!=sizeof(container)) return {};
    static constexpr GUID zero{};
    if(IsEqualGUID(container,zero)) return {};
    // Opaque comparison key, retained only in memory; never logged or serialized.
    return std::string(reinterpret_cast<const char*>(&container),sizeof(container));
#else
    (void)location; return {};
#endif
}
std::string KeyboardIdentity::Container(const std::string& location)
{
    const auto now=std::chrono::steady_clock::now();
    auto found=cache.find(location);
    if(found!=cache.end()&&now<found->second.until) return found->second.value;
    auto value=Resolve(location);
    if(cache.size()>=128) cache.clear();
    cache[location]={value,now+std::chrono::seconds(value.empty()?2:30)};
    return value;
}
}
