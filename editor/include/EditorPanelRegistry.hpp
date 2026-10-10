#pragma once
#include <functional>
#include <string>
#include <vector>
class EditorPanelRegistry {
public:
    struct Panel { std::string name;bool open=true;std::function<void()> draw; };
    void Register(std::string name,std::function<void()> draw) { m_panels.push_back({std::move(name),true,std::move(draw)}); }
    auto begin() noexcept { return m_panels.begin(); }
    auto end() noexcept { return m_panels.end(); }
private:
    std::vector<Panel> m_panels;
};
