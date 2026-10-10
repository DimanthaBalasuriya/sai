#include "engine/renderer/RenderGraph.hpp"
#include "ResourcePool.hpp"
#include <iostream>
namespace {
using namespace Engine;
using namespace Engine::Renderer;
int failures = 0;
void Check(bool condition, const char* name) { if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++failures; } }
Rhi::ImageDesc Image(std::string name = "Transient", std::uint32_t width = 64) {
    return {std::move(name),width,64,Rhi::Format::Rgba8Unorm,Rhi::ImageUsage::ColorAttachment | Rhi::ImageUsage::Sampled};
}
void HazardsAndBarriers() {
    RenderGraph graph;
    auto color = graph.CreateImage(Image());
    graph.AddPass({"Produce",{{color,Rhi::ResourceState::ColorWrite}},{},{}});
    graph.AddPass({"Read",{{color,Rhi::ResourceState::SampledRead}},{},{}});
    graph.AddPass({"Rewrite",{{color,Rhi::ResourceState::ColorWrite}},{},{}});
    graph.AddPass({"Rewrite again",{{color,Rhi::ResourceState::ColorWrite}},{},{}});
    CompiledGraph plan;
    Check(bool(graph.Compile(plan)),"compile RAW/WAR/WAW graph");
    if (plan.passes.size() != 4) { Check(false,"four compiled passes"); return; }
    for (std::uint32_t i = 0; i < 4; ++i) Check(plan.passes[i].index == i,"hazard order preserved");
    Check(plan.passes[0].transitions[0].discard,"discard first transient use");
    Check(plan.passes[1].transitions[0].before == Rhi::ResourceState::ColorWrite && plan.passes[1].transitions[0].after == Rhi::ResourceState::SampledRead,"write to read transition");
    Check(plan.passes[3].transitions.size() == 1,"same-state write barrier required");
    Check(!plan.passes[3].transitions[0].discard,"later write preserves previous dependencies");
}
void OrderingAndErrors() {
    CompiledGraph plan; RenderGraph explicitGraph;
    explicitGraph.AddPass({"Second",{},{},{1}}); explicitGraph.AddPass({"First",{},{},{}});
    Check(bool(explicitGraph.Compile(plan)) && plan.passes[0].index == 1,"explicit dependencies reorder independent passes");
    RenderGraph cycle; cycle.AddPass({"A",{},{},{1}}); cycle.AddPass({"B",{},{},{0}});
    Check(!cycle.Compile(plan),"cycle rejected");
    RenderGraph uninitialized; auto image = uninitialized.CreateImage(Image());
    uninitialized.AddPass({"Read",{{image,Rhi::ResourceState::SampledRead}},{},{}});
    Check(!uninitialized.Compile(plan),"uninitialized transient read rejected");
    RenderGraph invalid; invalid.AddPass({"Invalid",{{{99},Rhi::ResourceState::ColorWrite}},{},{}});
    Check(!invalid.Compile(plan),"invalid resource rejected");
    RenderGraph duplicate; auto d = duplicate.CreateImage(Image());
    duplicate.AddPass({"Duplicate",{{d,Rhi::ResourceState::ColorWrite},{d,Rhi::ResourceState::SampledRead}},{},{}});
    Check(!duplicate.Compile(plan),"duplicate use rejected");
    RenderGraph war; auto w = war.ImportImage("Imported",{0,1},Rhi::ResourceState::SampledRead);
    war.AddPass({"Read",{{w,Rhi::ResourceState::SampledRead}},{},{1}});
    war.AddPass({"Write",{{w,Rhi::ResourceState::ColorWrite}},{},{}});
    Check(!war.Compile(plan),"WAR hazard conflicting with explicit dependency rejected");
    RenderGraph wrongType; auto buffer = wrongType.ImportBuffer("Buffer",{0,1},Rhi::ResourceState::VertexRead);
    wrongType.AddPass({"Bad",{{buffer,Rhi::ResourceState::DepthWrite}},{},{}});
    Check(!wrongType.Compile(plan),"buffer image-state mismatch rejected");
}
void Aliasing() {
    RenderGraph graph;
    auto a = graph.CreateImage(Image("A")); auto b = graph.CreateImage(Image("B"));
    auto c = graph.CreateImage(Image("C")); auto incompatible = graph.CreateImage(Image("Wide",128));
    graph.AddPass({"A and B",{{a,Rhi::ResourceState::ColorWrite},{b,Rhi::ResourceState::ColorWrite}},{},{}});
    graph.AddPass({"Keep B",{{b,Rhi::ResourceState::SampledRead}},{},{}});
    graph.AddPass({"C and wide",{{c,Rhi::ResourceState::ColorWrite},{incompatible,Rhi::ResourceState::ColorWrite}},{},{}});
    CompiledGraph plan; Check(bool(graph.Compile(plan)),"compile transient lifetimes");
    Check(plan.allocationCount == 3,"three physical allocations for four logical images");
    Check(plan.allocationSlots[a.value] != plan.allocationSlots[b.value],"overlapping lifetimes cannot alias");
    Check(plan.allocationSlots[a.value] == plan.allocationSlots[c.value],"non-overlapping compatible images alias");
    Check(plan.allocationSlots[incompatible.value] != plan.allocationSlots[c.value],"incompatible extent cannot alias");
}
void Handles() {
    struct Record { explicit Record(int& count) : destroyed(&count) {} int* destroyed; ~Record() { ++*destroyed; } };
    int destroyed = 0; Rhi::ResourcePool<Record,Rhi::BufferHandle> pool;
    auto a = pool.Insert(std::make_unique<Record>(destroyed));
    auto* pointer = pool.Get(a);
    for (int i = 0; i < 100; ++i) pool.Insert(std::make_unique<Record>(destroyed));
    Check(pool.Get(a) == pointer,"record pointer survives pool growth");
    auto before = destroyed; Check(pool.Remove(a) && destroyed == before+1,"removal invokes RAII destructor");
    auto b = pool.Insert(std::make_unique<Record>(destroyed));
    Check(a.slot == b.slot && a.generation != b.generation,"reused slot changes generation");
    Check(!pool.Get(a) && !pool.Remove(a),"stale handles rejected");
    pool.Clear(); Check(!pool.Get(b) && destroyed == 102,"clear invalidates handles and destroys all records");
}
class Encoder final : public Rhi::CommandList {
public:
    std::vector<std::string> events;
    Result BeginPass(std::string_view name) override { events.emplace_back(name); return Result::Success(); }
    Result EndPass() override { events.emplace_back("End"); return Result::Success(); }
    Result Barriers(std::span<const Rhi::ImageBarrier> images, std::span<const Rhi::BufferBarrier>) override {
        for (const auto& image : images) events.push_back(image.after == Rhi::ResourceState::Present ? "Present barrier" : "Color barrier");
        return Result::Success();
    }
    Result BeginRendering(const Rhi::RenderingDesc&) override { return Result::Success(); }
    Result EndRendering() override { return Result::Success(); }
    Result BindPipeline(Rhi::PipelineHandle) override { return Result::Success(); }
    Result BindTextureTable(Rhi::TextureTableHandle) override { return Result::Success(); }
    Result BindMesh(Rhi::BufferHandle,Rhi::BufferHandle) override { return Result::Success(); }
    Result PushConstants(std::span<const std::byte>) override { return Result::Success(); }
    Result DrawIndexed(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t) override { return Result::Success(); }
};
void Execution() {
    // Device remains uninitialized: imported-resource execution requires no Vulkan instance/GPU.
    Rhi::Device device; TransientImageCache cache(device); Encoder commands; RenderGraph graph;
    const Rhi::ImageHandle imported{0,1}; auto color = graph.ImportImage("Color",imported,Rhi::ResourceState::Present);
    auto unused = graph.ImportImage("Undeclared",{1,1},Rhi::ResourceState::SampledRead);
    graph.AddPass({"Draw",{{color,Rhi::ResourceState::ColorWrite}},[&](Rhi::CommandList&,const PassResources& resources) {
        Check(resources.Image(color) == imported,"callback receives declared resource");
        Check(!resources.Image(unused) && !resources.Buffer(color),"callback cannot access undeclared or wrong-kind handles");
        commands.events.emplace_back("Callback"); return Result::Success();
    },{}});
    graph.AddPass({"Present",{{color,Rhi::ResourceState::Present}},{},{}});
    CompiledGraph plan; Check(bool(graph.Compile(plan)),"compile executable graph");
    Check(bool(graph.Execute(plan,commands,cache)),"execute with GPU-free encoder");
    Check(commands.events == std::vector<std::string>{"Draw","Color barrier","Callback","End","Present","Present barrier","End"},"barriers before callback and named pass boundaries");
    Check(graph.CpuPassMilliseconds().size() == 2,"CPU timings for every pass");
    commands.events.clear(); Check(bool(graph.Execute(plan,commands,cache)),"compiled plan can execute repeatedly");
    graph.AddPass({"New",{},{},{}}); commands.events.clear();
    Check(!graph.Execute(plan,commands,cache) && commands.events.empty(),"stale compiled plan rejected before recording");
    RenderGraph duplicate; duplicate.ImportImage("A",imported,Rhi::ResourceState::Present); duplicate.ImportImage("B",imported,Rhi::ResourceState::Present);
    Check(!duplicate.Compile(plan),"duplicate physical imports rejected");
    RenderGraph failed; failed.AddPass({"Throw",{},[](Rhi::CommandList&,const PassResources&) -> Result { throw std::runtime_error("record failure"); },{}});
    Check(bool(failed.Compile(plan)) && !failed.Execute(plan,commands,cache),"callback exception becomes Result failure");
}
}
int main() { HazardsAndBarriers(); OrderingAndErrors(); Aliasing(); Handles(); Execution();
    if (!failures) std::cout << "Render graph and resource lifetime tests passed\n"; return failures ? 1 : 0;
}
