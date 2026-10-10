#include "CallbackLayer.hpp"
namespace Engine::Sky {
std::unique_ptr<ISkyLayer> MakeAerialLayer(LayerDraw draw){return std::make_unique<CallbackLayer>("Aerial perspective",50,std::move(draw));}
}
