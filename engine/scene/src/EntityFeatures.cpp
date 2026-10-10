#include "engine/scene/EntityFeatures.hpp"
namespace Engine::Scene {
Result RegisterEntityFeatures(){
    const std::array flags{FieldInfo{"visible",FieldType::Unsigned,0,0,1},FieldInfo{"isStatic",FieldType::Unsigned,0,0,1},FieldInfo{"inheritVisible",FieldType::Unsigned,0,0,1},FieldInfo{"inheritStatic",FieldType::Unsigned,0,0,1}};
    const std::array<FieldValue,4> defaults{std::uint32_t{1},std::uint32_t{0},std::uint32_t{0},std::uint32_t{1}};
    World schema;bool flagsRegistered=false,inheritanceRegistered=false;
    for(auto& info:schema.Reflection()){
        if(info.type==kEntityFlags){if(info.name!="EntityFlags"||info.fields.size()!=flags.size())return Result::Failure("EntityFlags identity collision");flagsRegistered=true;}
        if(info.type==kPropertyInheritance){if(info.name!="PropertyInheritance")return Result::Failure("PropertyInheritance identity collision");inheritanceRegistered=true;}
    }
    if(!flagsRegistered){auto result=RegisterComponent(kEntityFlags,"EntityFlags",flags,defaults);if(!result)return result;}
    const std::array properties{FieldInfo{"meshColor",FieldType::Unsigned,0,0,1},FieldInfo{"lightColor",FieldType::Unsigned,0,0,1},FieldInfo{"lightIntensity",FieldType::Unsigned,0,0,1}};
    const std::array<FieldValue,3> none{std::uint32_t{0},std::uint32_t{0},std::uint32_t{0}};
    return inheritanceRegistered?Result::Success():RegisterComponent(kPropertyInheritance,"PropertyInheritance",properties,none);
}
}
