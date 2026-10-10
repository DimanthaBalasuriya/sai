#pragma once
#include "engine/extensions/Features.hpp"
namespace Engine::Extensions {
// CPU geometry owns vertices/indices; calls append and reject invalid shapes.
struct DebugVertex { Vec3 position,color; };
class DebugGeometry {
public:
    std::vector<DebugVertex> vertices;std::vector<std::uint32_t> indices;
    void Line(Vec3 start,Vec3 end,Vec3 color);
    void Box(const Mat4& transform,Vec3 color);
    Result Sphere(Vec3 center,float radius,Vec3 color,std::uint32_t segments=32);
};
std::unique_ptr<IModule> MakeDebugDrawModule(FeatureRegistry&,Rhi::Device&,const std::filesystem::path& shaders);
}
