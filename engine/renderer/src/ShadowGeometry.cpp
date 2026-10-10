#include "engine/renderer/ForwardPipeline.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cfloat>
namespace Engine::Renderer {
Result ForwardPipeline::PrepareShadows(const RenderWorld& world,std::uint32_t slot){
    bool full=m_shadowSource!=world.source||world.modified||(world.fullUpdate&&m_shadowRevision!=world.instanceRevision)||m_shadowProxies.size()!=world.instances.size();
    if(m_shadowRevision!=world.instanceRevision||full){
        if(full){m_shadowSpatial.Clear();m_shadowProxies.resize(world.instances.size());}
        auto update=[&](std::uint32_t i){auto& instance=world.instances[i];auto mesh=std::find_if(m_meshes.begin(),m_meshes.end(),[&](auto& m){return m.id==instance.mesh;});if(mesh==m_meshes.end())return;auto matrix=glm::make_mat4(instance.model.values.data());glm::vec3 lo(FLT_MAX),hi(-FLT_MAX);auto b=mesh->bounds;
            for(int c=0;c<8;++c){auto p=glm::vec3(matrix*glm::vec4(c&1?b.maximum.x:b.minimum.x,c&2?b.maximum.y:b.minimum.y,c&4?b.maximum.z:b.minimum.z,1));lo=glm::min(lo,p);hi=glm::max(hi,p);}Spatial::Aabb bounds{{lo.x,lo.y,lo.z},{hi.x,hi.y,hi.z}};if(full)m_shadowProxies[i]=m_shadowSpatial.Insert(bounds,i);else m_shadowSpatial.Update(m_shadowProxies[i],bounds);
        };if(full)for(std::uint32_t i=0;i<world.instances.size();++i)update(i);else for(auto i:world.changedInstances)update(i);
        m_shadowSource=world.source;m_shadowRevision=world.instanceRevision;
    }
    // Reuse the frame-model allocation; writes occur only when this fenced slot is stale.
    static_assert(sizeof(Mat4)==64);
    if(m_shadowUploadedSource[slot]!=world.source||m_shadowUploadedRevision[slot]!=world.instanceRevision||world.modified){
        m_shadowCpuModels.resize(world.instances.size());for(std::size_t i=0;i<world.instances.size();++i)m_shadowCpuModels[i]=world.instances[i].model;
        if(!m_shadowCpuModels.empty()){auto r=m_device.WriteBuffer(m_shadowModels[slot],std::as_bytes(std::span{m_shadowCpuModels}));if(!r)return r;}m_shadowUploadedSource[slot]=world.source;m_shadowUploadedRevision[slot]=world.instanceRevision;
    }
    for(std::uint32_t i=0;i<m_textures.size();++i){auto r=m_device.WriteTexture(m_shadowTables[slot],i,m_textures[i],m_sampler);if(!r)return r;}
    for(auto& texture:m_importTextures){auto r=m_device.WriteTexture(m_shadowTables[slot],texture.index,texture.image,m_sampler);if(!r)return r;}return Result::Success();
}
void ForwardPipeline::ShadowCasters(const Mat4& transform,std::vector<std::uint32_t>& out)const{
    auto vp=glm::make_mat4(transform.values.data());auto row=[&](int i){return glm::vec4(vp[0][i],vp[1][i],vp[2][i],vp[3][i]);};auto a=row(0),b=row(1),c=row(2),d=row(3);std::array raw{d+a,d-a,d+b,d-b,c,d-c};std::array<Spatial::Plane,6> planes;for(int i=0;i<6;++i)planes[i]={{raw[i].x,raw[i].y,raw[i].z},raw[i].w};m_shadowSpatial.Query(planes,out);
}
Result ForwardPipeline::AddShadowPass(RenderGraph& graph,const RenderWorld& world,Rhi::ImageHandle atlas,const Mat4& matrix,std::span<const std::uint32_t> candidates,std::uint32_t slot,std::uint32_t x,std::uint32_t y,std::uint32_t resolution,std::string name){
    auto depth=graph.ReuseImage("Shadow atlas",atlas,m_device.StateOf(atlas));auto models=graph.ReuseBuffer("Shadow models",m_shadowModels[slot],m_device.StateOf(m_shadowModels[slot]));auto materials=graph.ReuseBuffer("Shadow materials",m_materialBuffer,m_device.StateOf(m_materialBuffer));
    std::vector<ResourceUse> uses{{depth,Rhi::ResourceState::DepthWrite},{models,Rhi::ResourceState::StorageRead},{materials,Rhi::ResourceState::StorageRead}};
    for(auto& mesh:m_meshes){auto v=graph.ReuseBuffer("Shadow vertices",mesh.vertices,m_device.StateOf(mesh.vertices)),i=graph.ReuseBuffer("Shadow indices",mesh.indices,m_device.StateOf(mesh.indices));uses.push_back({v,Rhi::ResourceState::VertexRead});uses.push_back({i,Rhi::ResourceState::IndexRead});}
    for(auto image:m_textures){auto id=graph.ReuseImage("Shadow alpha",image,m_device.StateOf(image));uses.push_back({id,Rhi::ResourceState::SampledRead});}for(auto& texture:m_importTextures){auto id=graph.ReuseImage("Shadow alpha",texture.image,m_device.StateOf(texture.image));uses.push_back({id,Rhi::ResourceState::SampledRead});}
    std::vector<std::uint32_t> owned(candidates.begin(),candidates.end());
    graph.AddPass({std::move(name),std::move(uses),[this,&world,atlas,matrix,owned=std::move(owned),slot,x,y,resolution](Rhi::CommandList& cmd,const PassResources&){
        Rhi::RenderingDesc rendering;rendering.depth=atlas;rendering.clearDepth=0;rendering.x=x;rendering.y=y;rendering.width=rendering.height=resolution;auto r=cmd.BeginRendering(rendering);if(r)r=cmd.BindPipeline(m_shadowPipeline);if(r)r=cmd.BindTextureTable(m_shadowTables[slot]);if(!r)return r;
        for(auto index:owned){const auto& instance=world.instances[index];auto mesh=std::find_if(m_meshes.begin(),m_meshes.end(),[&](auto& m){return m.id==instance.mesh;});if(mesh==m_meshes.end())continue;auto material=std::find_if(m_materials.begin(),m_materials.end(),[&](auto& m){return m.id==instance.material;});if(material==m_materials.end())continue;
            struct Push {Mat4 matrix;std::uint64_t models,materials;std::uint32_t index,material,pad0=0,pad1=0;} push{matrix,m_device.BufferAddress(m_shadowModels[slot]),m_device.BufferAddress(m_materialBuffer),index,std::uint32_t(material-m_materials.begin())};static_assert(sizeof(Push)==96);
            r=cmd.BindMesh(mesh->vertices,mesh->indices);if(r)r=cmd.PushConstants(std::as_bytes(std::span{&push,1}));if(r)r=cmd.DrawIndexed(mesh->indexCount);if(!r)return r;
        }return cmd.EndRendering();
    },{}});return Result::Success();
}
}
