#pragma once
#include "engine/core/Result.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
namespace EditorInput {
enum class Context { Global,Viewport,Hierarchy,ViewportFlying,Interaction,TextInput,Count };
enum Modifier : unsigned { Ctrl=1,Shift=2,Alt=4 };
enum class Trigger { Press,Release,Hold,Double,Delta };
struct Chord {std::string key;unsigned modifiers=0;bool operator==(const Chord&) const=default;};
struct Binding {std::string action;Context context;Chord chord;Trigger trigger=Trigger::Press;bool anyModifiers=false;};
struct KeyState {bool down=false,pressed=false,released=false,doubleClick=false;};
struct Frame {
    std::unordered_map<std::string,KeyState> keys;unsigned modifiers=0;
    float x=0,y=0,dx=0,dy=0,wheel=0,delta=.016f;bool textInput=false,focused=true;
};
Engine::Result ParseChord(std::string_view,Chord&);
std::string FormatChord(const Chord&);
Engine::Result ValidateBindings(const std::vector<Binding>&);
Engine::Result LoadBindings(const std::filesystem::path&,std::vector<Binding>&);
class Actions {
public:
    Engine::Result Load(const std::filesystem::path&);
    Engine::Result Rebind(std::vector<Binding>);
    void Route(const Frame&,const std::array<bool,static_cast<std::size_t>(Context::Count)>&);
    bool Active(std::string_view action) const;
    bool Requested(const Frame&,std::string_view action) const;
    void Queue(std::string action);
    std::string Shortcut(std::string_view action) const;
    const std::vector<Binding>& Bindings() const {return m_bindings;}
private:
    std::vector<Binding> m_bindings;std::vector<std::string> m_pending;
    std::unordered_map<std::string,bool> m_active;
};
}
