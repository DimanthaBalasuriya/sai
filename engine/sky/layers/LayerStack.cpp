#include "engine/sky/Layers.hpp"
#include <algorithm>
namespace Engine::Sky {
Result LayerStack::Add(std::unique_ptr<ISkyLayer> layer){if(!layer||!layer->Name()||!*layer->Name())return Result::Failure("Missing sky layer");for(auto& entry:m_entries)if(entry.layer->Name()==std::string_view(layer->Name()))return Result::Failure("Duplicate sky layer");if(m_started){auto r=layer->Initialize();if(!r)return r;}int priority=layer->Priority();m_entries.push_back({std::move(layer),true,priority});return Result::Success();}
Result LayerStack::Remove(std::string_view name){auto it=std::find_if(m_entries.begin(),m_entries.end(),[&](auto& e){return e.layer->Name()==name;});if(it==m_entries.end())return Result::Failure("Unknown sky layer");m_entries.erase(it);return Result::Success();}
Result LayerStack::SetPriority(std::string_view name,int priority){for(auto& e:m_entries)if(e.layer->Name()==name){e.priority=priority;return Result::Success();}return Result::Failure("Unknown sky layer");}
bool LayerStack::SetEnabled(std::string_view name,bool enabled){for(auto& e:m_entries)if(e.layer->Name()==name){e.enabled=enabled;return true;}return false;}
Result LayerStack::Initialize(){if(m_started)return Result::Success();for(auto& e:m_entries){auto r=e.layer->Initialize();if(!r)return r;}m_started=true;return Result::Success();}
Result LayerStack::Declare(Renderer::RenderGraph& graph,SkyContext& context){std::vector<Entry*> ordered;for(auto& e:m_entries)if(e.enabled)ordered.push_back(&e);std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->priority<b->priority;});for(auto e:ordered){auto r=e->layer->Declare(graph,context);if(!r)return r;e->layer->DebugViews(context.debug);}return Result::Success();}
Result SkyRenderFeature::AddPasses(Renderer::RenderGraph& graph,Extensions::FeatureFrame& frame){if(!frame.lit)return Result::Success();m_context.frame=&frame;m_context.debug.views.clear();auto result=m_layers.Declare(graph,m_context);m_context.frame=nullptr;return result;}
}
