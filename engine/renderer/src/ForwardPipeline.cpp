#include "engine/renderer/ForwardPipeline.hpp"
#include "engine/core/Log.hpp"
#include "engine/renderer/Projection.hpp"
#include "engine/renderer/Exposure.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <exception>
#include <map>
#include <tuple>
#include <cstring>
#include "engine/core/Profile.hpp"
#include <limits>
#include <cfloat>
#include <glm/gtc/type_ptr.hpp>
namespace Engine::Renderer {
namespace {
struct Vertex { float x,y,z,u,v,nx=0,ny=0,nz=0; };
struct GpuInstance { glm::mat4 model,normal; glm::vec4 color; };
struct LegacyGpuLight { glm::vec4 positionRange,colorIntensity; };
struct GpuScene { glm::mat4 viewProjection; std::array<LegacyGpuLight,4> directional{}; std::array<LegacyGpuLight,16> points{}; glm::uvec4 counts{}; glm::vec4 cameraPosition{}; glm::mat4 worldToShadow{1}; glm::vec4 environment{}; std::uint64_t lightsAddress=0;std::uint32_t directionalCount=0,localCount=0; std::uint64_t frameAddress=0;std::uint64_t irradiance=0;glm::vec4 ambient{}; };
struct GpuMaterial { glm::vec4 baseColor,factors,emissiveAlpha; glm::uvec4 textures,extra; };
struct DrawData { std::uint64_t scene,instances,materials; std::uint32_t materialIndex,debugView; };
static_assert(sizeof(GpuInstance)==144 && sizeof(GpuScene)==864 && sizeof(DrawData)==32 && sizeof(GpuMaterial)==80);
class OpaqueGeometry final : public IRenderFeature {
public: Result AddPasses(RenderGraph& graph,const ForwardFrame& frame) override {
    return graph.AddPass(frame.geometry)==UINT32_MAX ? Result::Failure("Failed to declare opaque geometry") : Result::Success();
}
};
constexpr std::array<Vertex,24> kVertices{{
    {-1,-1,1,0,0},{1,-1,1,1,0},{1,1,1,1,1},{-1,1,1,0,1},
    {1,-1,-1,0,0},{-1,-1,-1,1,0},{-1,1,-1,1,1},{1,1,-1,0,1},
    {-1,-1,-1,0,0},{-1,-1,1,1,0},{-1,1,1,1,1},{-1,1,-1,0,1},
    {1,-1,1,0,0},{1,-1,-1,1,0},{1,1,-1,1,1},{1,1,1,0,1},
    {-1,1,1,0,0},{1,1,1,1,0},{1,1,-1,1,1},{-1,1,-1,0,1},
    {-1,-1,-1,0,0},{1,-1,-1,1,0},{1,-1,1,1,1},{-1,-1,1,0,1}
}};
constexpr std::array<std::uint32_t,36> kIndices{{
    0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,
    12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20
}};
}
ForwardPipeline::ForwardPipeline(Rhi::Device& device) : m_device(device) {
    for (auto& cache : m_caches) cache = std::make_unique<TransientImageCache>(device);
    m_features.push_back(std::make_unique<OpaqueGeometry>());
}
ForwardPipeline::~ForwardPipeline() {
    if(m_shadowPipeline)m_device.Destroy(m_shadowPipeline);for(auto b:m_shadowModels)if(b)m_device.Destroy(b);for(auto t:m_shadowTables)if(t)m_device.Destroy(t);
    const auto destroy = [&](auto handle) { if (handle) { auto result = m_device.Destroy(handle); if (!result) Log::Error(result.Error()); } };
    for (auto buffer:m_instanceBuffers) destroy(buffer);
    for (auto buffer:m_sceneBuffers) destroy(buffer);
    destroy(m_materialBuffer);destroy(m_neutralEnvironment);
    for (const auto& mesh:m_meshes) if (mesh.id!=kBuiltinCube) { destroy(mesh.vertices); destroy(mesh.indices); }
    for (const auto& texture:m_importTextures) destroy(texture.image);
    for(auto table:m_tables) destroy(table);
    for(auto depth:m_depth) destroy(depth.image);for(auto image:m_retired) destroy(image); destroy(m_pipeline); destroy(m_wirePipeline); destroy(m_vertices); destroy(m_indices);
    for (auto handle : m_textures) destroy(handle); destroy(m_sampler);destroy(m_shadowSampler);destroy(m_iblSampler);
}
Result ForwardPipeline::Initialize(const std::filesystem::path& shaders,Rhi::Format outputFormat) {
    try {
        auto vertexData=kVertices;
        constexpr std::array<glm::vec3,6> normals{{{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}}};
        for (std::size_t i=0;i<vertexData.size();++i) { vertexData[i].nx=normals[i/4].x; vertexData[i].ny=normals[i/4].y; vertexData[i].nz=normals[i/4].z; }
        if (m_vertices) return Result::Failure("Mesh demo already initialized");
        auto result = m_device.CreateBuffer({"Cube vertices",sizeof(kVertices), Rhi::BufferUsage::Vertex | Rhi::BufferUsage::TransferDestination | Rhi::BufferUsage::DeviceAddress,false},m_vertices);
        if (!result) return result;
        result = m_device.UploadBuffer(m_vertices,std::as_bytes(std::span{vertexData}),Rhi::ResourceState::VertexRead); if (!result) return result;
        result = m_device.CreateBuffer({"Cube indices",sizeof(kIndices),Rhi::BufferUsage::Index | Rhi::BufferUsage::TransferDestination,false},m_indices); if (!result) return result;
        result = m_device.UploadBuffer(m_indices,std::as_bytes(std::span{kIndices}),Rhi::ResourceState::IndexRead); if (!result) return result;
        if (m_device.BufferAddress(m_vertices) == 0) return Result::Failure("Vertex buffer device address unavailable");
        result = m_device.CreateSampler({"Material sampler",true,true},m_sampler);if(result)result=m_device.CreateSampler({"Shadow point clamp",false,false},m_shadowSampler);if(result)result=m_device.CreateSampler({"IBL clamp",true,false},m_iblSampler); if (!result) return result;
        for(auto& table:m_tables) { result=m_device.CreateTextureTable("Frame material textures",table);if(!result)return result; }
        for (std::uint32_t index = 0; index < m_textures.size(); ++index) {
            result = m_device.CreateImage({"Checker texture " + std::to_string(index),64,64,Rhi::Format::Rgba8Srgb,Rhi::ImageUsage::Sampled | Rhi::ImageUsage::TransferDestination},m_textures[index]); if (!result) return result;
            std::array<std::uint8_t,64*64*4> pixels{};
            for (std::size_t y = 0; y < 64; ++y) for (std::size_t x = 0; x < 64; ++x) {
                const bool light = ((x/8 + y/8) % 2) == 0; auto offset = (y*64+x)*4;
                pixels[offset] = light ? 235 : (index == 0 ? 35 : 175);
                pixels[offset+1] = light ? 235 : (index == 0 ? 135 : 55);
                pixels[offset+2] = light ? 225 : (index == 0 ? 225 : 30);
                pixels[offset+3] = 255;
            }
            result = m_device.UploadImage(m_textures[index],std::as_bytes(std::span{pixels})); if (!result) return result;
            for(auto table:m_tables) { result=m_device.SetTexture(table,index,m_textures[index],m_sampler);if(!result)return result; }
        }
        Rhi::ImageDesc neutral{"Neutral environment",1,1,Rhi::Format::Rgba8Unorm,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::TransferDestination};neutral.dimension=Rhi::ImageDimension::Cube;
        result=m_device.CreateImage(neutral,m_neutralEnvironment);if(!result)return result;std::array<std::uint8_t,24> black{};result=m_device.UploadImage(m_neutralEnvironment,std::as_bytes(std::span{black}));if(!result)return result;
        Rhi::PipelineDesc pipeline;
        pipeline.name = "Instanced lit forward"; pipeline.vertexShader = shaders / "forward.vertex.spv"; pipeline.fragmentShader = shaders / "forward.fragment.spv";
        pipeline.vertexStride = sizeof(Vertex); pipeline.pushConstantBytes = sizeof(DrawData); pipeline.depthTest = true; pipeline.reverseZ=true; pipeline.colorFormat=outputFormat;
        pipeline.attributes = {{0,0,Rhi::VertexFormat::Float3},{1,12,Rhi::VertexFormat::Float2},{2,20,Rhi::VertexFormat::Float3}};
        result=m_device.CreatePipeline(pipeline,m_pipeline); if (!result) return result;
        auto shadow=pipeline;shadow.name="Shadow depth";shadow.depthOnly=true;shadow.attributes.resize(2);shadow.pushConstantBytes=96;shadow.vertexShader=shaders.parent_path()/"extensions/shadow.vertex.spv";shadow.fragmentShader=shaders.parent_path()/"extensions/shadow.fragment.spv";shadow.depthBias=-1;shadow.slopeBias=-1.5f;result=m_device.CreatePipeline(shadow,m_shadowPipeline);if(!result)return result;
        pipeline.wireframe=true;pipeline.name="Wireframe";result=m_device.CreatePipeline(pipeline,m_wirePipeline);if (!result) return result;
        for (std::size_t i=0;i<Rhi::kFramesInFlight;++i) {
            result=m_device.CreateBuffer({"Shadow models",kCapacity*sizeof(Mat4),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},m_shadowModels[i]);if(!result)return result;
            result=m_device.CreateTextureTable("Shadow alpha materials",m_shadowTables[i]);if(!result)return result;
            result=m_device.CreateBuffer({"Frame instances "+std::to_string(i),kCapacity*sizeof(GpuInstance),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},m_instanceBuffers[i]); if (!result) return result;
            result=m_device.CreateBuffer({"Frame camera and lights "+std::to_string(i),sizeof(GpuScene),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress,true},m_sceneBuffers[i]); if (!result) return result;
        }
        m_meshes.push_back({kBuiltinCube,m_vertices,m_indices,36});
        MaterialAsset builtin; builtin.id=kBuiltinMaterial; builtin.name="Builtin checker"; builtin.metallic=0; builtin.roughness=0.8f;
        m_materials.push_back(builtin);
        result=m_device.CreateBuffer({"PBR material data",256*sizeof(GpuMaterial),Rhi::BufferUsage::Storage|Rhi::BufferUsage::DeviceAddress|Rhi::BufferUsage::TransferDestination,false},m_materialBuffer); if (!result) return result;
        return UploadAssets({});
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result ForwardPipeline::UploadAssets(const AssetBundle& bundle) {
    try {
        if (bundle.textures.size()+m_importTextures.size()+2>Rhi::kTextureTableCapacity-3 || bundle.materials.size()+m_materials.size()>256) return Result::Failure("Asset bundle exceeds indexed resource capacity");
        for (const auto& mesh:bundle.meshes) {
            if (std::any_of(m_meshes.begin(),m_meshes.end(),[&](const auto& value){return value.id==mesh.id;})) continue;
            if (mesh.vertices.empty()||mesh.indices.empty()) return Result::Failure("Empty mesh asset");
            GpuMesh gpu{mesh.id,{},{},static_cast<std::uint32_t>(mesh.indices.size())};
            gpu.bounds.minimum={FLT_MAX,FLT_MAX,FLT_MAX};gpu.bounds.maximum={-FLT_MAX,-FLT_MAX,-FLT_MAX};for(auto v:mesh.vertices){gpu.bounds.minimum={std::min(gpu.bounds.minimum.x,v.x),std::min(gpu.bounds.minimum.y,v.y),std::min(gpu.bounds.minimum.z,v.z)};gpu.bounds.maximum={std::max(gpu.bounds.maximum.x,v.x),std::max(gpu.bounds.maximum.y,v.y),std::max(gpu.bounds.maximum.z,v.z)};}
            auto result=m_device.CreateBuffer({mesh.name+" vertices",mesh.vertices.size()*sizeof(MeshVertex),Rhi::BufferUsage::Vertex|Rhi::BufferUsage::TransferDestination,false},gpu.vertices); if (!result) return result;
            result=m_device.UploadBuffer(gpu.vertices,std::as_bytes(std::span{mesh.vertices}),Rhi::ResourceState::VertexRead); if (!result) return result;
            result=m_device.CreateBuffer({mesh.name+" indices",mesh.indices.size()*4,Rhi::BufferUsage::Index|Rhi::BufferUsage::TransferDestination,false},gpu.indices); if (!result) return result;
            result=m_device.UploadBuffer(gpu.indices,std::as_bytes(std::span{mesh.indices}),Rhi::ResourceState::IndexRead); if (!result) return result;
            m_meshes.push_back(gpu);
        }
        for (const auto& texture:bundle.textures) {
            if (std::any_of(m_importTextures.begin(),m_importTextures.end(),[&](const auto& value){return value.id==texture.id;})) continue;
            GpuTexture gpu{texture.id,{},static_cast<std::uint32_t>(2+m_importTextures.size())};
            auto result=m_device.CreateImage({texture.name,texture.width,texture.height,texture.srgb?Rhi::Format::Rgba8Srgb:Rhi::Format::Rgba8Unorm,Rhi::ImageUsage::Sampled|Rhi::ImageUsage::TransferDestination},gpu.image); if (!result) return result;
            result=m_device.UploadImage(gpu.image,std::as_bytes(std::span{texture.rgba})); if (!result) return result;
            for(auto table:m_tables) { result=m_device.SetTexture(table,gpu.index,gpu.image,m_sampler);if(!result)return result; }
            m_importTextures.push_back(gpu);
        }
        for (const auto& material:bundle.materials) if (std::none_of(m_materials.begin(),m_materials.end(),[&](const auto& value){return value.id==material.id;})) m_materials.push_back(material);
        const auto textureIndex=[&](Guid id) -> std::uint32_t {
            if (!id) return UINT32_MAX;
            for (const auto& texture:m_importTextures) if (texture.id==id) return texture.index;
            throw std::runtime_error("Material references an unloaded texture GUID");
        };
        std::vector<GpuMaterial> materials;
        for (const auto& material:m_materials) {
            GpuMaterial gpu{{material.baseColor[0],material.baseColor[1],material.baseColor[2],material.baseColor[3]},
                {material.metallic,material.roughness,material.normalScale,material.occlusionStrength},{material.emissive.x,material.emissive.y,material.emissive.z,material.alphaCutoff},
                {material.id==kBuiltinMaterial?0u:textureIndex(material.baseColorTexture),textureIndex(material.metallicRoughnessTexture),textureIndex(material.normalTexture),textureIndex(material.emissiveTexture)},
                {textureIndex(material.occlusionTexture),material.alphaMode,material.doubleSided?1u:0u,0}};
            materials.push_back(gpu);
        }
        return m_device.UploadBuffer(m_materialBuffer,std::as_bytes(std::span{materials}),Rhi::ResourceState::StorageRead);
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}

Result ForwardPipeline::AddFeature(std::unique_ptr<IRenderFeature> feature) {
    try { if (!feature) return Result::Failure("Null render feature"); m_features.push_back(std::move(feature)); return Result::Success(); }
    catch (const std::exception& error) { return Result::Failure(error.what()); }
}
Result ForwardPipeline::PrepareFrame() {
    for(auto image:m_retired) {auto r=m_device.Destroy(image);if(!r)return r;}m_retired.clear();
    for (auto& cache:m_caches) { auto result=cache->FlushRetired();if (!result) return result; }return Result::Success();
}
Result ForwardPipeline::Render(const RenderWorld& world) {
    auto result=PrepareFrame();if (!result) return result;
    Rhi::FrameInfo frame;bool available=false;result=m_device.BeginFrame(frame,available);if (!result||!available) return result;
    result=RenderToTarget(world,frame,true);if (!result) return result;return m_device.EndFrame();
}
Result ForwardPipeline::RenderToTarget(const RenderWorld& world,const Rhi::FrameInfo& frame,bool present) {
    try {
        ENGINE_PROFILE_SCOPE("Renderer.InstanceHandoff");
        if(!world.camera.valid||world.instances.size()>kCapacity)return Result::Failure("Forward scene needs a camera and at most 131072 instances");
        bool changed=world.modified||m_batchSource!=world.source||m_batchRevision!=world.revision;
        bool incremental=changed&&m_batchSource==world.source&&m_batchRevision==world.baseRevision&&!world.fullUpdate&&m_proxies.size()==world.instances.size();
        bool keysChanged=false;
        auto meshBounds=[&](std::uint32_t index){auto& instance=world.instances[index];auto found=std::find_if(m_meshes.begin(),m_meshes.end(),[&](auto& m){return m.id==instance.mesh;});if(found==m_meshes.end())throw std::runtime_error("Unregistered mesh GUID");auto b=found->bounds;auto m=glm::make_mat4(instance.model.values.data());glm::vec3 minimum(FLT_MAX),maximum(-FLT_MAX);for(int c=0;c<8;++c){auto p=glm::vec3(m*glm::vec4(c&1?b.maximum.x:b.minimum.x,c&2?b.maximum.y:b.minimum.y,c&4?b.maximum.z:b.minimum.z,1));minimum=glm::min(minimum,p);maximum=glm::max(maximum,p);}return Spatial::Aabb{{minimum.x,minimum.y,minimum.z},{maximum.x,maximum.y,maximum.z}};};
        if(changed){
            ENGINE_PROFILE_SCOPE("Renderer.SpatialUpdate");
            if(!incremental){++m_spatialGeneration;if(m_batchSource!=world.source){m_spatial.Clear();m_spatialEntries.clear();}m_proxies.resize(world.instances.size());m_keys.resize(world.instances.size());m_query.reserve(world.instances.size());m_visible.reserve(world.instances.size());for(std::uint32_t i=0;i<world.instances.size();++i){auto id=world.instances[i].entity;auto found=m_spatialEntries.find(id);if(found==m_spatialEntries.end())m_proxies[i]=m_spatialEntries.emplace(id,SpatialEntry{m_spatial.Insert(meshBounds(i),i),m_spatialGeneration}).first->second.proxy;else{m_proxies[i]=found->second.proxy;found->second.mark=m_spatialGeneration;m_spatial.SetPayload(m_proxies[i],i);m_spatial.Update(m_proxies[i],meshBounds(i));}m_keys[i]={world.instances[i].mesh,world.instances[i].material};}for(auto it=m_spatialEntries.begin();it!=m_spatialEntries.end();)if(it->second.mark!=m_spatialGeneration){m_spatial.Remove(it->second.proxy);it=m_spatialEntries.erase(it);}else ++it;}
            else for(auto i:world.changedInstances){if(i>=world.instances.size())return Result::Failure("Invalid instance dirty index");auto key=std::pair{world.instances[i].mesh,world.instances[i].material};keysChanged|=key!=m_keys[i];m_keys[i]=key;m_spatial.Update(m_proxies[i],meshBounds(i));}
        }
        auto cullProjection=PerspectiveReverseZ(world.camera.verticalFov,static_cast<float>(frame.width)/frame.height,world.camera.nearPlane,world.camera.farPlane);
        auto vp=cullProjection*glm::inverse(glm::make_mat4(world.camera.transform.values.data()));Mat4 cullMatrix;std::copy_n(glm::value_ptr(vp),16,cullMatrix.values.begin());
        bool visibilityChanged=false;
        if(!m_cullValid||cullMatrix.values!=m_cullMatrix.values||(changed&&(!incremental||!world.changedInstances.empty()))){
            auto row=[&](int i){return glm::vec4(vp[0][i],vp[1][i],vp[2][i],vp[3][i]);};auto r0=row(0),r1=row(1),r2=row(2),r3=row(3);std::array raw{r3+r0,r3-r0,r3+r1,r3-r1,r2,r3-r2};std::array<Spatial::Plane,6> planes;for(int i=0;i<6;++i)planes[i]={{raw[i].x,raw[i].y,raw[i].z},raw[i].w};m_spatial.Query(planes,m_query);std::sort(m_query.begin(),m_query.end());visibilityChanged=m_query!=m_visible;m_visible.swap(m_query);m_cullMatrix=cullMatrix;m_cullValid=true;
        }
        bool rebuild=visibilityChanged||keysChanged||(changed&&!incremental);
        auto gpuValue=[&](std::uint32_t index){auto& instance=world.instances[index];return GpuInstance{glm::make_mat4(instance.model.values.data()),glm::make_mat4(instance.normal.values.data()),{instance.color.x,instance.color.y,instance.color.z,1}};};
        if(rebuild){
            std::map<std::tuple<std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t>,std::vector<std::uint32_t>> groups;
            for(auto i:m_visible){auto& instance=world.instances[i];if(std::none_of(m_materials.begin(),m_materials.end(),[&](auto& m){return m.id==instance.material;}))return Result::Failure("Unregistered material GUID");groups[{instance.mesh.high,instance.mesh.low,instance.material.high,instance.material.low}].push_back(i);}
            m_mapping.assign(world.instances.size(),UINT32_MAX);m_batches.clear();m_instanceBytes.resize(m_visible.size()*sizeof(GpuInstance));std::uint32_t offset=0;
            for(auto& [key,group]:groups){auto& first=world.instances[group.front()];m_batches.push_back({first.mesh,first.material,offset,static_cast<std::uint32_t>(group.size())});for(auto index:group){auto value=gpuValue(index);std::memcpy(m_instanceBytes.data()+offset*sizeof(GpuInstance),&value,sizeof(value));m_mapping[index]=offset++;}}
            for(std::size_t slot=0;slot<Rhi::kFramesInFlight;++slot){m_uploaded[slot]=false;m_pending[slot].clear();m_pending[slot].reserve(m_visible.size());m_pendingFlags[slot].assign(m_visible.size(),0);}
        }else if(incremental){for(auto index:world.changedInstances){auto gpuIndex=m_mapping[index];if(gpuIndex==UINT32_MAX)continue;auto value=gpuValue(index);std::memcpy(m_instanceBytes.data()+gpuIndex*sizeof(GpuInstance),&value,sizeof(value));for(std::size_t slot=0;slot<Rhi::kFramesInFlight;++slot)if(!m_pendingFlags[slot][gpuIndex]){m_pendingFlags[slot][gpuIndex]=1;m_pending[slot].push_back(gpuIndex);}}}
        m_batchSource=world.source;m_batchRevision=world.revision;
        std::size_t directional=0,points=0;
        if(!world.lighting.address)for (const auto& light:world.lights) {
            if(light.modulePacked&&(light.gpu.flags&kLightEnabled)==0)continue;
            if (light.type==0) ++directional; else if (light.type==1&&!light.modulePacked) ++points; else if(!light.modulePacked)return Result::Failure("Legacy spot shading is deferred");
        }
        if (directional>4 || points>16) return Result::Failure("Forward lighting supports four directional and sixteen point lights");
        auto result=Result::Success();
        const auto slot=frame.frameSlot;
        if(!m_instanceBytes.empty()){
            if(!m_uploaded[slot]){result=m_device.WriteBuffer(m_instanceBuffers[slot],m_instanceBytes);if(!result)return result;}
            else{auto& pending=m_pending[slot];std::sort(pending.begin(),pending.end());for(std::size_t i=0;i<pending.size();){auto begin=pending[i],end=begin+1;++i;while(i<pending.size()&&pending[i]==end){++end;++i;}auto offset=static_cast<std::size_t>(begin)*sizeof(GpuInstance);result=m_device.WriteBuffer(m_instanceBuffers[slot],std::span<const std::byte>(m_instanceBytes).subspan(offset,(end-begin)*sizeof(GpuInstance)),offset);if(!result)return result;}}
            for(auto index:m_pending[slot])m_pendingFlags[slot][index]=0;m_pending[slot].clear();m_uploaded[slot]=true;
        }
        GpuScene scene{};
        auto projection=PerspectiveReverseZ(world.camera.verticalFov,static_cast<float>(frame.width)/frame.height,world.camera.nearPlane,world.camera.farPlane);
        auto cameraTransform=glm::make_mat4(world.camera.transform.values.data());
        auto rotationOnly=cameraTransform;rotationOnly[3]={0,0,0,1};scene.viewProjection=projection*glm::inverse(rotationOnly); scene.cameraPosition=cameraTransform[3]; scene.cameraPosition.w=ManualExposure(world.camera.ev100,world.camera.exposureCompensation);
        if(!world.lighting.address)for (const auto& light:world.lights) {
            if(light.modulePacked&&(light.gpu.flags&kLightEnabled)==0)continue;
            if (light.type==0) { scene.directional[scene.counts.y++]={{light.direction.x,light.direction.y,light.direction.z,0},{light.color.x*light.intensity,light.color.y*light.intensity,light.color.z*light.intensity,0}}; }
            else if (light.type==1&&!light.modulePacked) {
                if (scene.counts.x>=scene.points.size()) return Result::Failure("M3 supports at most 16 point lights");
                scene.points[scene.counts.x++]={{light.position.x,light.position.y,light.position.z,light.range},{light.color.x,light.color.y,light.color.z,light.intensity}};
            } else if(!light.modulePacked)return Result::Failure("Legacy spot shading is deferred");
        }
        scene.frameAddress=world.lighting.frameAddress;scene.lightsAddress=world.lighting.address;scene.directionalCount=world.lighting.directionalCount;scene.localCount=world.lighting.localCount;
        scene.irradiance=world.environment.irradianceAddress;scene.ambient={world.settings.ambient.x,world.settings.ambient.y,world.settings.ambient.z,world.environment.brdf?1.f:0.f};scene.worldToShadow=glm::make_mat4(world.environment.worldToShadow.values.data());
        scene.environment={world.environment.diffuseCube?1.f:0.f,world.environment.shadow?1.f:0.f,world.environment.intensity,world.environment.wetness};
        result=m_device.WriteTexture(m_tables[slot],0,world.environment.diffuseCube?world.environment.diffuseCube:m_neutralEnvironment,m_sampler);if(!result)return result;
        for(std::uint32_t i=0;i<5;++i)if(world.environment.specular[i]){result=m_device.WriteTexture(m_tables[slot],i+1,world.environment.specular[i],m_sampler);if(!result)return result;}if(world.environment.brdf){result=m_device.WriteTexture(m_tables[slot],253,world.environment.brdf,m_iblSampler);if(!result)return result;}if(world.lighting.shadowAtlas){result=m_device.WriteTexture(m_tables[slot],254,world.lighting.shadowAtlas,m_shadowSampler);if(!result)return result;}
        result=m_device.WriteTexture(m_tables[slot],255,world.environment.shadow?world.environment.shadow:m_textures[1],m_sampler);if(!result)return result;
        result=m_device.WriteBuffer(m_sceneBuffers[slot],std::as_bytes(std::span{&scene,1})); if (!result) return result;
        RenderGraph graph;
        auto color=graph.ReuseImage("Window color",frame.color,m_device.StateOf(frame.color));
        auto& targetDepth=m_depth[slot];
        if(!targetDepth.image||targetDepth.width!=frame.width||targetDepth.height!=frame.height){Rhi::ImageHandle image;result=m_device.CreateImage({"Forward sampled depth",frame.width,frame.height,Rhi::Format::D32Float,Rhi::ImageUsage::DepthAttachment|Rhi::ImageUsage::Sampled},image);if(!result)return result;if(targetDepth.image)m_retired.push_back(targetDepth.image);targetDepth={image,frame.width,frame.height};}
        auto depth=graph.ReuseImage("Forward depth",targetDepth.image,m_device.StateOf(targetDepth.image));
        std::vector<ResourceId> meshVertices,meshIndices;
        for (const auto& mesh:m_meshes) { meshVertices.push_back(graph.ReuseBuffer("Mesh vertices",mesh.vertices,m_device.StateOf(mesh.vertices))); meshIndices.push_back(graph.ReuseBuffer("Mesh indices",mesh.indices,m_device.StateOf(mesh.indices))); }
        auto materials=graph.ReuseBuffer("PBR materials",m_materialBuffer,m_device.StateOf(m_materialBuffer));
        auto instances=graph.ReuseBuffer("Instances",m_instanceBuffers[slot],m_device.StateOf(m_instanceBuffers[slot]));
        auto lighting=graph.ReuseBuffer("Camera and lights",m_sceneBuffers[slot],m_device.StateOf(m_sceneBuffers[slot]));
        auto texture=graph.ReuseImage("Checker",m_textures[0],m_device.StateOf(m_textures[0]));
        std::vector<ResourceUse> uses{{color,Rhi::ResourceState::ColorWrite},{depth,Rhi::ResourceState::DepthWrite},{materials,Rhi::ResourceState::StorageRead},{lighting,Rhi::ResourceState::StorageRead},{texture,Rhi::ResourceState::SampledRead}};
        for(auto buffer:{world.lighting.frameBuffer,world.lighting.clusterBounds,world.lighting.clusterCells,world.lighting.clusterIndices,world.lighting.shadowBuffer,world.environment.irradiance,world.lighting.areaBuffer,world.lighting.ltcBuffer})if(buffer){auto id=graph.ReuseBuffer("Lighting resources",buffer,m_device.StateOf(buffer));uses.push_back({id,Rhi::ResourceState::StorageRead});}
        if(world.lighting.buffer){auto id=graph.ReuseBuffer("Physical light buffer",world.lighting.buffer,m_device.StateOf(world.lighting.buffer));uses.push_back({id,Rhi::ResourceState::StorageRead});}
        for(auto image:{m_textures[1],m_neutralEnvironment}){auto id=graph.ReuseImage("Default lighting descriptor",image,m_device.StateOf(image));uses.push_back({id,Rhi::ResourceState::SampledRead});}
        for (std::size_t i=0;i<m_meshes.size();++i) { uses.push_back({meshVertices[i],Rhi::ResourceState::VertexRead}); uses.push_back({meshIndices[i],Rhi::ResourceState::IndexRead}); }
        for (const auto& importedTexture:m_importTextures) { auto id=graph.ReuseImage("Material texture",importedTexture.image,m_device.StateOf(importedTexture.image)); uses.push_back({id,Rhi::ResourceState::SampledRead}); }
        for(auto image:{world.environment.diffuseCube,world.environment.shadow,world.lighting.shadowAtlas})if(image){auto id=graph.ReuseImage("Environment lighting",image,m_device.StateOf(image));uses.push_back({id,Rhi::ResourceState::SampledRead});}
        for(auto image:world.environment.specular)if(image){auto id=graph.ReuseImage("Prefiltered specular",image,m_device.StateOf(image));uses.push_back({id,Rhi::ResourceState::SampledRead});}if(world.environment.brdf){auto id=graph.ReuseImage("Integrated BRDF",world.environment.brdf,m_device.StateOf(world.environment.brdf));uses.push_back({id,Rhi::ResourceState::SampledRead});}if (!world.instances.empty()) uses.push_back({instances,Rhi::ResourceState::StorageRead});
        GraphPass geometry{"Opaque geometry",std::move(uses),[&](Rhi::CommandList& commands,const PassResources& resources) {
            auto recorded=commands.BeginRendering({resources.Image(color),resources.Image(depth),{0.015f*scene.cameraPosition.w,0.02f*scene.cameraPosition.w,0.035f*scene.cameraPosition.w,0},0.f}); if (!recorded) return recorded;
            if (!world.instances.empty()) {
                recorded=commands.BindPipeline(m_debugView==4?m_wirePipeline:m_pipeline); if (!recorded) return recorded;
                recorded=commands.BindTextureTable(m_tables[slot]); if (!recorded) return recorded;
                for (const auto& batch:m_batches) {
                    std::size_t meshIndex=0; while (m_meshes[meshIndex].id!=batch.mesh) ++meshIndex;
                    std::uint32_t materialIndex=0; while (m_materials[materialIndex].id!=batch.material) ++materialIndex;
                    recorded=commands.BindMesh(resources.Buffer(meshVertices[meshIndex]),resources.Buffer(meshIndices[meshIndex])); if (!recorded) return recorded;
                    DrawData draw{m_device.BufferAddress(resources.Buffer(lighting)),m_device.BufferAddress(resources.Buffer(instances))+static_cast<std::uint64_t>(batch.first)*sizeof(GpuInstance),m_device.BufferAddress(resources.Buffer(materials)),materialIndex,m_debugView};
                    recorded=commands.PushConstants(std::as_bytes(std::span{&draw,1})); if (!recorded) return recorded;
                    recorded=commands.DrawIndexed(m_meshes[meshIndex].indexCount,0,batch.count,0); if (!recorded) return recorded;
                }
            }
            return commands.EndRendering();
        },{}};
        ForwardFrame context{color,depth,frame.width,frame.height,std::move(geometry)};
        for (const auto& feature:m_features) { result=feature->AddPasses(graph,context); if (!result) return result; }
        graph.AddPass({present?"Present":"Viewport ready",{{color,present?Rhi::ResourceState::Present:Rhi::ResourceState::SampledRead}},{},{}});
        graph.AddPass({"Depth ready",{{depth,Rhi::ResourceState::SampledRead}},{},{}});
        CompiledGraph plan; result=graph.Compile(plan); if (!result) return result;
        result=graph.Execute(plan,m_device.Commands(),*m_caches[slot]); if (!result) return result;
        m_cpuPassTimes.clear();for (std::size_t i=0;i<graph.Passes().size();++i) m_cpuPassTimes.emplace_back(graph.Passes()[i].name,graph.CpuPassMilliseconds()[i]);
        m_lastInstanceCount=world.instances.size();return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}

