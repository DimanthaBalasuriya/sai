#include "engine/sky/GpuSky.hpp"
namespace Engine::Sky {
namespace {
class Aurora final:public ISkyLayer {
public:
    explicit Aurora(std::function<Result(Renderer::RenderGraph&,SkyContext&)> draw):m_draw(std::move(draw)){}
    const char* Name()const noexcept override{return "Aurora";}int Priority()const noexcept override{return 35;}
    Result Declare(Renderer::RenderGraph& graph,SkyContext& context)override{return m_draw(graph,context);}
private:std::function<Result(Renderer::RenderGraph&,SkyContext&)> m_draw;
};
}
std::unique_ptr<ISkyLayer> MakeAuroraLayer(std::function<Result(Renderer::RenderGraph&,SkyContext&)> draw){return std::make_unique<Aurora>(std::move(draw));}
}
