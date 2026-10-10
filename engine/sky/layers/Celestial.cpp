#include "CallbackLayer.hpp"
namespace Engine::Sky {
std::unique_ptr<ISkyLayer> MakeCelestialLayer(LayerDraw draw){return std::make_unique<CallbackLayer>("Celestial bodies",20,std::move(draw));}
}
