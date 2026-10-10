#include "CallbackLayer.hpp"
namespace Engine::Sky {
std::unique_ptr<ISkyLayer> MakeCloudsLayer(LayerDraw draw){return std::make_unique<CallbackLayer>("Volumetric clouds",40,std::move(draw));}
}
