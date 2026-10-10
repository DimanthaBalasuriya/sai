#include "CallbackLayer.hpp"
namespace Engine::Sky {
std::unique_ptr<ISkyLayer> MakeCubemapLayer(LayerDraw draw){return std::make_unique<CallbackLayer>("SkyboxCubemap",5,std::move(draw));}
}
