#include "CallbackLayer.hpp"
namespace Engine::Sky {
std::unique_ptr<ISkyLayer> MakeAtmosphereLayer(LayerDraw draw){return std::make_unique<CallbackLayer>("Atmosphere",10,std::move(draw));}
}
