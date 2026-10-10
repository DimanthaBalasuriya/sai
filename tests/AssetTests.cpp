#include "engine/assets/Registry.hpp"
#include <iostream>
int main() {
    using namespace Engine;int failures=0;
    const auto check=[&](bool condition,const char* message){if (!condition) { ++failures;std::cerr<<"FAIL: "<<message<<'\n'; }};
    Assets::Registry registry("asset-test-cache");check(bool(registry.Open()),"open registry");
    Assets::ImportedScene imported;auto result=registry.Import(std::filesystem::path(ENGINE_TEST_SAMPLES)/"material_gallery.gltf",imported);
    check(bool(result),"import multi-mesh fixture");if (!result) { std::cerr<<result.Error();return 1; }
    check(imported.assets.meshes.size()==3&&imported.assets.materials.size()==4&&imported.assets.textures.size()==2,"primitive/material/texture counts");
    check(imported.assets.textures[0].srgb&&!imported.assets.textures[1].srgb,"base color sRGB and metallic/roughness linear");
    auto id=imported.id;auto meshId=imported.assets.meshes[0].id;
    check(bool(registry.Import(std::filesystem::path(ENGINE_TEST_SAMPLES)/"material_gallery.gltf",imported))&&imported.id==id&&imported.assets.meshes[0].id==meshId,"stable identities across reimport");
    Assets::Registry reopened("asset-test-cache");check(bool(reopened.Open()),"reopen persisted catalog");Assets::ImportedScene cooked;
    check(bool(reopened.Load(id,cooked))&&cooked.assets.meshes[2].indices==imported.assets.meshes[2].indices&&cooked.assets.textures[0].rgba==imported.assets.textures[0].rgba,"cooked binary round trip");
    Scene::World world;Scene::CommandHistory history;check(bool(Assets::Instantiate(cooked,world,history)),"instantiate imported hierarchy through commands");
    Renderer::RenderWorld snapshot;check(bool(world.Extract(snapshot))&&snapshot.instances.size()==6,"all primitive instances extracted");
    check(!registry.Import("missing.gltf",imported),"missing source rejected");check(imported.id==id,"failed import preserves output");
    check(!reopened.Load({9,9},cooked),"unknown asset GUID rejected");
    if (!failures) std::cout<<"Asset import, GUID persistence, cook and instantiation tests passed\n";return failures?1:0;
}
