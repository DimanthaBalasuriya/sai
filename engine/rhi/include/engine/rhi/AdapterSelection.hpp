#pragma once
#include "engine/core/Result.hpp"
#include <charconv>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
namespace Engine::Rhi {
enum class AdapterClass { Other, Software, Virtual, Integrated, Discrete };
struct AdapterInfo {
    std::uint32_t index=0;
    std::string name;
    AdapterClass type=AdapterClass::Other;
    std::uint64_t localMemoryBytes=0;
    std::string missing;
    std::uint32_t driverVersion=0, graphicsFamily=0, presentFamily=0;
};
// Pure policy: stable enumeration indices, no API calls and no software fallback.
inline Result SelectAdapter(std::span<const AdapterInfo> adapters,std::string_view overrideName,std::size_t& selected) {
    selected=adapters.size();
    unsigned requested=0;
    bool numeric=false;
    if(!overrideName.empty()){auto [end,ec]=std::from_chars(overrideName.data(),overrideName.data()+overrideName.size(),requested);numeric=ec==std::errc{}&&end==overrideName.data()+overrideName.size();}
    std::string diagnostic="No suitable hardware GPU (requires Vulkan 1.3, dynamic rendering, synchronization2, buffer device address, descriptor indexing, graphics/present queues).";
    for(std::size_t i=0;i<adapters.size();++i){
        const auto& a=adapters[i];
        diagnostic+="\n["+std::to_string(a.index)+"] "+a.name+": "+(a.type==AdapterClass::Software?"software renderer rejected":a.missing.empty()?"compatible":a.missing);
        if(a.type==AdapterClass::Software||a.type==AdapterClass::Other||!a.missing.empty())continue;
        if(!overrideName.empty() && !(numeric?a.index==requested:a.name==overrideName))continue;
        if(selected==adapters.size()||a.type>adapters[selected].type||(a.type==adapters[selected].type&&a.localMemoryBytes>adapters[selected].localMemoryBytes))selected=i;
    }
    if(selected!=adapters.size())return Result::Success();
    if(!overrideName.empty())diagnostic+="\nRequested adapter: "+std::string(overrideName);
    return Result::Failure(diagnostic);
}
}
