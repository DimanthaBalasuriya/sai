#pragma once
#include "engine/sky/GpuSky.hpp"
namespace Engine::Sky {
using LayerDraw=std::function<Result(Renderer::RenderGraph&,SkyContext&)>;
class CallbackLayer:public ISkyLayer {
public:CallbackLayer(const char* name,int order,LayerDraw draw):m_name(name),m_order(order),m_draw(std::move(draw)){}
 const char* Name()const noexcept override{return m_name;}int Priority()const noexcept override{return m_order;}
 Result Declare(Renderer::RenderGraph& graph,SkyContext& context)override{return m_draw(graph,context);}
private:const char* m_name;int m_order;LayerDraw m_draw;
};
std::unique_ptr<ISkyLayer> MakeAtmosphereLayer(LayerDraw);
std::unique_ptr<ISkyLayer> MakeCelestialLayer(LayerDraw);
std::unique_ptr<ISkyLayer> MakeStarsLayer(LayerDraw);
std::unique_ptr<ISkyLayer> MakeCloudsLayer(LayerDraw);
std::unique_ptr<ISkyLayer> MakeAerialLayer(LayerDraw);
std::unique_ptr<ISkyLayer> MakeCubemapLayer(LayerDraw);
}
