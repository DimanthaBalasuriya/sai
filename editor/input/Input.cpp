#include "Input.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
namespace EditorInput {
namespace {
std::string Canonical(std::string_view name) {
    std::string text(name);std::erase_if(text,[](unsigned char c){return std::isspace(c)!=0;});for(auto& c:text) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if(text.size()==1&&std::isalnum(static_cast<unsigned char>(text[0]))) return text;
    for(auto key:{"Ctrl","Shift","Alt","LMB","RMB","MMB","Scroll","MouseDelta","Escape","Delete","Tab","Space","Enter","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"}) {
        std::string upper(key);for(auto& c:upper)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));if(upper==text) return key;
    }return {};
}
unsigned Mod(std::string_view key) {return key=="Ctrl"?Ctrl:key=="Shift"?Shift:key=="Alt"?Alt:0;}
bool Matches(const Binding& b,unsigned modifiers) {modifiers&=~Mod(b.chord.key);return b.anyModifiers?(modifiers&b.chord.modifiers)==b.chord.modifiers:modifiers==b.chord.modifiers;}
int Priority(Context context) {switch(context) {case Context::TextInput:return 1000;case Context::Interaction:return 500;case Context::ViewportFlying:return 400;case Context::Hierarchy:return 300;case Context::Viewport:return 200;default:return 100;}}
}
Engine::Result ParseChord(std::string_view text,Chord& output) {
    Chord next;std::size_t begin=0;
    while(begin<text.size()) {auto end=text.find('+',begin);if(end==std::string_view::npos) end=text.size();auto key=Canonical(text.substr(begin,end-begin));
        if(key.empty()) return Engine::Result::Failure("Unknown input in chord: "+std::string(text));
        if(end<text.size()) {auto mod=Mod(key);if(!mod||next.modifiers&mod) return Engine::Result::Failure("Invalid or repeated chord modifier");next.modifiers|=mod;}
        else next.key=key;begin=end+1;
    }
    if(next.key.empty()||(next.modifiers&Mod(next.key))) return Engine::Result::Failure("Chord requires a key");output=std::move(next);return Engine::Result::Success();
}
std::string FormatChord(const Chord& chord) {std::string s;if(chord.modifiers&Ctrl)s+="Ctrl+";if(chord.modifiers&Shift)s+="Shift+";if(chord.modifiers&Alt)s+="Alt+";return s+chord.key;}
Engine::Result ValidateBindings(const std::vector<Binding>& bindings) {
    for(std::size_t i=0;i<bindings.size();++i) {
        const auto& a=bindings[i];if(a.action.empty()||Canonical(a.chord.key)!=a.chord.key||a.chord.key.empty()||a.context<Context::Global||a.context>=Context::Count||a.chord.modifiers>7||a.trigger<Trigger::Press||a.trigger>Trigger::Delta) return Engine::Result::Failure("Invalid action binding");
        for(std::size_t j=0;j<i;++j) {const auto& b=bindings[j];
            if(a.context==b.context&&a.chord.key==b.chord.key&&a.trigger==b.trigger) for(unsigned modifiers=0;modifiers<8;++modifiers) if(Matches(a,modifiers)&&Matches(b,modifiers)) return Engine::Result::Failure("Conflicting bindings: "+a.action+" and "+b.action);
        }
    }return Engine::Result::Success();
}
Engine::Result LoadBindings(const std::filesystem::path& path,std::vector<Binding>& output) {
    try {std::ifstream file(path);if(!file) return Engine::Result::Failure("Cannot open bindings: "+path.string());nlohmann::json json;file>>json;if(json.at("version")!=1) return Engine::Result::Failure("Unsupported bindings version");std::vector<Binding> next;
        const std::unordered_map<std::string,Context> contexts{{"Global",Context::Global},{"Viewport",Context::Viewport},{"Hierarchy",Context::Hierarchy},{"ViewportFlying",Context::ViewportFlying},{"Interaction",Context::Interaction},{"TextInput",Context::TextInput}};
        const std::unordered_map<std::string,Trigger> triggers{{"press",Trigger::Press},{"release",Trigger::Release},{"hold",Trigger::Hold},{"double",Trigger::Double},{"delta",Trigger::Delta}};
        for(const auto& item:json.at("bindings")) {Binding b;b.action=item.at("action");b.context=contexts.at(item.at("context").get<std::string>());auto result=ParseChord(item.at("chord").get<std::string>(),b.chord);if(!result) return result;b.trigger=triggers.at(item.value("trigger",std::string("press")));b.anyModifiers=item.value("anyModifiers",false);next.push_back(std::move(b));}
        auto result=ValidateBindings(next);if(result) output=std::move(next);return result;
    }catch(const std::exception& error) {return Engine::Result::Failure(std::string("Bindings: ")+error.what());}
}
Engine::Result Actions::Load(const std::filesystem::path& path) {std::vector<Binding> next;auto result=LoadBindings(path,next);return result?Rebind(std::move(next)):result;}
Engine::Result Actions::Rebind(std::vector<Binding> bindings) {auto result=ValidateBindings(bindings);if(result) m_bindings=std::move(bindings);return result;}
bool Actions::Active(std::string_view action) const {auto it=m_active.find(std::string(action));return it!=m_active.end()&&it->second;}
bool Actions::Requested(const Frame& frame,std::string_view action) const {for(const auto& b:m_bindings) if(b.action==action) {auto it=frame.keys.find(b.chord.key);if(it!=frame.keys.end()&&it->second.down&&Matches(b,frame.modifiers))return true;}return false;}
void Actions::Queue(std::string action) {m_pending.push_back(std::move(action));}
std::string Actions::Shortcut(std::string_view action) const {std::string result;std::set<std::string> seen;for(const auto& b:m_bindings) if(b.action==action&&b.chord.key!="MouseDelta") {auto chord=FormatChord(b.chord);if(!seen.insert(chord).second)continue;if(!result.empty())result+=" / ";result+=chord;}return result;}
void Actions::Route(const Frame& frame,const std::array<bool,static_cast<std::size_t>(Context::Count)>& enabled) {
    m_active.clear();std::set<std::string> consumed;
    std::vector<const Binding*> ordered;for(const auto& b:m_bindings) if(enabled[static_cast<std::size_t>(b.context)]) ordered.push_back(&b);
    std::stable_sort(ordered.begin(),ordered.end(),[](auto* a,auto* b){return Priority(a->context)>Priority(b->context);});
    if(frame.focused) for(const auto* b:ordered) {
        bool pointer=b->chord.key=="LMB"||b->chord.key=="RMB"||b->chord.key=="MMB"||b->chord.key=="Scroll"||b->chord.key=="MouseDelta";
        if(frame.textInput&&b->context!=Context::TextInput&&!pointer) continue;
        if(consumed.contains(b->chord.key)) continue;
        if(!Matches(*b,frame.modifiers)) continue;
        auto it=frame.keys.find(b->chord.key);KeyState key=it==frame.keys.end()?KeyState{}:it->second;
        bool active=b->trigger==Trigger::Press?key.pressed:b->trigger==Trigger::Release?key.released:b->trigger==Trigger::Hold?key.down:b->trigger==Trigger::Double?key.doubleClick:b->chord.key=="Scroll"?frame.wheel!=0:frame.dx!=0||frame.dy!=0;
        if(active) {m_active[b->action]=true;consumed.insert(b->chord.key);}
    }
    for(auto& action:m_pending) m_active[action]=true;m_pending.clear();
}
}
