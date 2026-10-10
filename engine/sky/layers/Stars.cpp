#include "CallbackLayer.hpp"
namespace Engine::Sky {
std::unique_ptr<ISkyLayer> MakeStarsLayer(LayerDraw draw){return std::make_unique<CallbackLayer>("Stars",30,std::move(draw));}
}
