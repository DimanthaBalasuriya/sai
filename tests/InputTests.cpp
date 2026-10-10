#include "Input.h"
#include "Camera.h"
#include <iostream>
#include <fstream>
#include <cmath>
using namespace EditorInput;
namespace {
int failures=0;
void Check(bool value,const char* message) {if(!value){++failures;std::cerr<<message<<'\n';}}
Binding Bind(const char* action,Context context,const char* text,Trigger trigger=Trigger::Press,bool any=false) {Chord chord;ParseChord(text,chord);return {action,context,chord,trigger,any};}
}
int main() {
    Chord chord;Check(bool(ParseChord(" ctrl + SHIFT + k ",chord))&&FormatChord(chord)=="Ctrl+Shift+K","normalize chord");
    auto before=chord;for(auto invalid:{"","Ctrl+","Ctrl+Ctrl+K","Ctrl+Unknown","W+E","+K"})Check(!ParseChord(invalid,chord)&&chord==before,"invalid chord is transactional");
    Check(bool(ParseChord("Alt+LMB",chord))&&chord.key=="LMB","mouse chord");Check(bool(ParseChord("Scroll",chord)),"scroll chord");
    Actions actions;auto loaded=actions.Load(ENGINE_TEST_BINDINGS);if(!loaded)std::cerr<<loaded.Error()<<'\n';Check(bool(loaded),"default binding table validates");
    Check(actions.Shortcut("Camera.Focus")=="F","duplicate context shortcuts collapse");
    std::array<bool,static_cast<std::size_t>(Context::Count)> contexts{};contexts[0]=contexts[1]=true;
    Frame frame;frame.keys["Q"]={true,true};actions.Route(frame,contexts);Check(actions.Active("Tool.Select")&&!actions.Active("Camera.FlyDown"),"Q selects outside flying");
    contexts[static_cast<std::size_t>(Context::ViewportFlying)]=true;actions.Route(frame,contexts);Check(actions.Active("Camera.FlyDown")&&!actions.Active("Tool.Select"),"flying context consumes Q");
    frame.keys.clear();frame.keys["E"]={true,true};actions.Route(frame,contexts);Check(actions.Active("Camera.FlyUp")&&!actions.Active("Tool.Rotate"),"flying consumes E");
    frame.textInput=true;frame.keys["D"]={true,true};frame.modifiers=Ctrl;actions.Route(frame,contexts);Check(!actions.Active("Edit.Duplicate")&&!actions.Active("Camera.FlyUp"),"text input blocks global and camera hotkeys");
    contexts[static_cast<std::size_t>(Context::TextInput)]=true;frame.keys.clear();frame.keys["Escape"]={true,true};frame.modifiers=0;actions.Route(frame,contexts);Check(actions.Active("Text.Cancel")&&!actions.Active("Edit.Cancel"),"text context handles escape");
    frame.textInput=false;frame.focused=false;frame.keys["D"]={true,true};frame.modifiers=Ctrl;actions.Route(frame,contexts);Check(!actions.Active("Edit.Duplicate"),"unfocused input ignored");
    actions.Queue("Edit.Copy");actions.Route(frame,contexts);Check(actions.Active("Edit.Copy"),"explicit menu action remains usable");actions.Route(frame,contexts);Check(!actions.Active("Edit.Copy"),"queued action fires once");
    Check(!ValidateBindings({Bind("A",Context::Global,"Ctrl+K"),Bind("B",Context::Global,"Ctrl+K")}),"same context conflict rejected");
    Check(bool(ValidateBindings({Bind("A",Context::Global,"K"),Bind("B",Context::Viewport,"K")})),"different contexts can share key");
    Check(!ValidateBindings({Bind("A",Context::Global,"Ctrl+K",Trigger::Hold,true),Bind("B",Context::Global,"Ctrl+Shift+K",Trigger::Hold)}),"wildcard modifier overlap rejected");
    Check(bool(ValidateBindings({Bind("A",Context::Global,"Ctrl+K",Trigger::Hold,true),Bind("B",Context::Global,"K",Trigger::Hold)})),"required wildcard modifier preserves distinct chord");
    auto bindings=actions.Bindings();bindings.push_back(bindings.front());Check(!actions.Rebind(bindings)&&actions.Shortcut("Tool.Move")=="W","failed rebind preserves existing table");
    auto path=std::filesystem::temp_directory_path()/"engine-f2-bindings-test.json";
    {std::ofstream file(path);file<<R"({"version":1,"bindings":[{"action":"Tool.Move","context":"Viewport","chord":"T"}]})";}
    Actions restarted;Check(bool(restarted.Load(path))&&restarted.Shortcut("Tool.Move")=="T","restart loads changed chord and label");frame={};frame.keys["T"]={true,true};restarted.Route(frame,contexts);Check(restarted.Active("Tool.Move"),"rebound chord dispatches");
    {std::ofstream file(path);file<<R"({"version":1,"bindings":[{"action":"A","context":"Global","chord":"K"},{"action":"B","context":"Global","chord":"K"}]})";}
    Check(!restarted.Load(path)&&restarted.Shortcut("Tool.Move")=="T","bad JSON binding table does not partially replace");std::filesystem::remove(path);
    Camera camera;camera.Focus({3,4,5},2);Actions idle;Frame tick;tick.delta=.025f;for(int i=0;i<12;++i)camera.Update(idle,tick,800);
    Check(!camera.Focusing()&&glm::length(camera.focus-glm::vec3(3,4,5))<.001f,"focus animation finishes at target");
    Camera flying;Actions movement;movement.Rebind({Bind("Camera.FlyUp",Context::ViewportFlying,"E",Trigger::Hold),Bind("Camera.Look",Context::ViewportFlying,"RMB",Trigger::Hold)});Frame input;input.keys["E"]={true};input.keys["RMB"]={true};auto eye=flying.Eye();movement.Route(input,contexts);flying.Update(movement,input,800);Check(flying.Eye().y>eye.y,"fly up moves camera");
    return failures?1:0;
}
