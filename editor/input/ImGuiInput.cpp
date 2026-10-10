#include "ImGuiInput.h"
#include <imgui.h>
#include <imgui_internal.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace EditorInput {
namespace {
struct Key {const char* name;ImGuiKey imgui;};
constexpr Key keys[]{{"Escape",ImGuiKey_Escape},{"Delete",ImGuiKey_Delete},{"Tab",ImGuiKey_Tab},{"Space",ImGuiKey_Space},{"Enter",ImGuiKey_Enter},{"Ctrl",ImGuiKey_LeftCtrl},{"Shift",ImGuiKey_LeftShift},{"Alt",ImGuiKey_LeftAlt}};
ImGuiKey NativeKey(std::string_view name) {
    if(name.size()==1&&name[0]>='A'&&name[0]<='Z') return static_cast<ImGuiKey>(ImGuiKey_A+name[0]-'A');
    if(name.size()==1&&name[0]>='0'&&name[0]<='9') return static_cast<ImGuiKey>(ImGuiKey_0+name[0]-'0');
    if(name[0]=='F'&&name.size()<=3) {int n=std::stoi(std::string(name.substr(1)));if(n>=1&&n<=12)return static_cast<ImGuiKey>(ImGuiKey_F1+n-1);}
    for(auto key:keys)if(name==key.name)return key.imgui;return ImGuiKey_None;
}
}
void ImGuiInput::Attach(GLFWwindow* window) {
    m_window=window;m_previousUser=glfwGetWindowUserPointer(window);glfwSetWindowUserPointer(window,this);
    m_previousScroll=glfwSetScrollCallback(window,[](GLFWwindow* w,double x,double y){auto* self=static_cast<ImGuiInput*>(glfwGetWindowUserPointer(w));self->m_scroll+=y;if(self->m_previousScroll)self->m_previousScroll(w,x,y);});
}
void ImGuiInput::Detach() {if(m_window) {CapturePointer(false);glfwSetScrollCallback(m_window,m_previousScroll);glfwSetWindowUserPointer(m_window,m_previousUser);m_window=nullptr;}}
ImGuiInput::~ImGuiInput() {Detach();}
void ImGuiInput::CapturePointer(bool enabled) {
    if(enabled==m_captured||!m_window)return;m_captured=enabled;glfwSetInputMode(m_window,GLFW_CURSOR,enabled?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
    if(glfwRawMouseMotionSupported())glfwSetInputMode(m_window,GLFW_RAW_MOUSE_MOTION,enabled?GLFW_TRUE:GLFW_FALSE);
    if(enabled)ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NoMouse;else ImGui::GetIO().ConfigFlags&=~ImGuiConfigFlags_NoMouse;
    glfwGetCursorPos(m_window,&m_lastX,&m_lastY);
}
Frame ImGuiInput::Capture(bool flying) {
    auto& io=ImGui::GetIO();Frame frame;frame.delta=io.DeltaTime;frame.focused=glfwGetWindowAttrib(m_window,GLFW_FOCUSED)!=0;
    frame.textInput=!flying&&(io.WantTextInput||(GImGui->InputTextState.ID!=0&&GImGui->InputTextState.ID==GImGui->ActiveId)||GImGui->OpenPopupStack.Size>0);
    frame.modifiers=(io.KeyCtrl?Ctrl:0)|(io.KeyShift?Shift:0)|(io.KeyAlt?Alt:0);
    auto add=[&](std::string name,ImGuiKey key){frame.keys[std::move(name)]={ImGui::IsKeyDown(key),ImGui::IsKeyPressed(key,false),ImGui::IsKeyReleased(key),false};};
    for(int i=0;i<26;++i)add(std::string(1,static_cast<char>('A'+i)),static_cast<ImGuiKey>(ImGuiKey_A+i));
    for(int i=0;i<10;++i)add(std::to_string(i),static_cast<ImGuiKey>(ImGuiKey_0+i));
    for(int i=0;i<12;++i)add("F"+std::to_string(i+1),static_cast<ImGuiKey>(ImGuiKey_F1+i));for(auto key:keys)add(key.name,key.imgui);
    frame.keys["Ctrl"].down=io.KeyCtrl;frame.keys["Shift"].down=io.KeyShift;frame.keys["Alt"].down=io.KeyAlt;
    const char* mouse[]{"LMB","RMB","MMB"};for(int i=0;i<3;++i) {bool down=flying?glfwGetMouseButton(m_window,i)==GLFW_PRESS:io.MouseDown[i];frame.keys[mouse[i]]={down,down&&!m_buttons[i],!down&&m_buttons[i],!flying&&ImGui::IsMouseDoubleClicked(i)};m_buttons[i]=down;}
    if(flying) {double x,y;glfwGetCursorPos(m_window,&x,&y);frame.dx=float(x-m_lastX);frame.dy=float(y-m_lastY);frame.x=float(x);frame.y=float(y);m_lastX=x;m_lastY=y;frame.wheel=float(m_scroll);}
    else {frame.x=io.MousePos.x;frame.y=io.MousePos.y;frame.dx=io.MouseDelta.x;frame.dy=io.MouseDelta.y;frame.wheel=io.MouseWheel;}m_scroll=0;
    if(m_smokePointer) {auto& sample=*m_smokePointer;frame.x=sample.x;frame.y=sample.y;frame.dx=sample.dx;frame.dy=sample.dy;frame.wheel=sample.wheel;for(auto name:{"LMB","RMB","MMB"})frame.keys[name]=sample.keys[name];m_smokePointer.reset();}
    return frame;
}
bool ImGuiInput::ItemClicked() const {return ImGui::IsItemClicked(ImGuiMouseButton_Left);}
bool ImGuiInput::ItemDoubleClicked() const {return ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);}
void ImGuiInput::Inject(std::string_view text,bool down) {
    Chord chord;if(!ParseChord(text,chord))return;auto& io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,down&&((chord.modifiers&Ctrl)||chord.key=="Ctrl"));io.AddKeyEvent(ImGuiMod_Shift,down&&((chord.modifiers&Shift)||chord.key=="Shift"));io.AddKeyEvent(ImGuiMod_Alt,down&&((chord.modifiers&Alt)||chord.key=="Alt"));
    if(chord.key=="LMB"||chord.key=="RMB"||chord.key=="MMB") {io.AddMouseButtonEvent(chord.key=="LMB"?0:chord.key=="RMB"?1:2,down);return;}
    auto key=NativeKey(chord.key);if(key!=ImGuiKey_None)io.AddKeyEvent(key,down);
}
}
