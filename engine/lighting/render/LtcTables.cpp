#include "LtcTables.hpp"
namespace Engine::Lighting {
namespace {struct mat33{float values[9];};
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4305)
#endif
// Heitz, Dupuy, Hill, Neubelt, Real-Time Polygonal-Light Shading with
// Linearly Transformed Cosines, SIGGRAPH 2016. License in third_party/ltc.
#include "../third_party/ltc/ltc.inc"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}
std::vector<LtcEntry> LtcTables(){std::vector<LtcEntry> result(size*size);for(int i=0;i<size*size;++i){auto& m=tabMinv[i].values;float scale=m[4];result[i]={{m[0]/scale,m[2]/scale,m[6]/scale,m[8]/scale},{tabMagnitude[i],0,0,0}};}return result;}
}
