#include "engine/assets/Registry.hpp"
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <functional>
#include <cstring>
namespace Engine::Assets {
namespace {
Guid Derived(Guid source,std::uint64_t domain,std::uint64_t index) { return {source.high^(domain*0x9e3779b97f4a7c15ull),source.low^(index+1)}; }
std::vector<std::uint8_t> Base64(std::string_view text) {
    constexpr std::string_view alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::uint8_t> bytes; std::uint32_t bits=0; int available=0;
    for (char c:text) { if (c=='=') break; auto digit=alphabet.find(c); if (digit==std::string_view::npos) throw std::runtime_error("Invalid base64 image"); bits=(bits<<6)|static_cast<std::uint32_t>(digit); available+=6; if (available>=8) { available-=8; bytes.push_back(static_cast<std::uint8_t>(bits>>available)); } }
    return bytes;
}
std::vector<std::uint8_t> ImageBytes(const cgltf_image& image,const std::filesystem::path& root) {
    if (image.buffer_view) {
        auto* data=static_cast<const std::uint8_t*>(cgltf_buffer_view_data(image.buffer_view));
        if (!data) throw std::runtime_error("Image buffer is missing"); return {data,data+image.buffer_view->size};
    }
    if (!image.uri) throw std::runtime_error("Image has no source");
    std::string uri=image.uri;
    if (uri.starts_with("data:")) { auto comma=uri.find(','); if (comma==std::string::npos||uri.substr(0,comma).find(";base64")==std::string::npos) throw std::runtime_error("Only base64 image data URIs are supported"); return Base64(std::string_view(uri).substr(comma+1)); }
    cgltf_decode_uri(uri.data()); uri.resize(std::strlen(uri.c_str()));
    std::ifstream stream(root/uri,std::ios::binary|std::ios::ate); if (!stream) throw std::runtime_error("Cannot read texture: "+uri);
    auto size=stream.tellg(); if (size<=0||size>256*1024*1024) throw std::runtime_error("Texture file size invalid");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size)); stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()),size); if (!stream) throw std::runtime_error("Texture read failed"); return bytes;
}
}
Result ImportGltf(const std::filesystem::path& path,Guid source,ImportedScene& output) {
    try {
        cgltf_options options{}; cgltf_data* raw=nullptr;
        if (cgltf_parse_file(&options,path.string().c_str(),&raw)!=cgltf_result_success) return Result::Failure("Invalid glTF/GLB: "+path.string());
        std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
        if (cgltf_load_buffers(&options,raw,path.string().c_str())!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success) return Result::Failure("glTF buffers/accessors failed validation");
        if (raw->skins_count||raw->animations_count) return Result::Failure("Animated/skinned glTF is outside the static M4 importer");
        ImportedScene scene; scene.id=source; scene.name=path.stem().string();
        std::unordered_map<const cgltf_texture*,std::array<Guid,2>> textures;
        const auto texture=[&](const cgltf_texture_view& view,bool srgb) -> Guid {
            if (!view.texture) return {};
            if (view.texcoord!=0||view.has_transform) throw std::runtime_error("Only UV0 without texture transforms is supported");
            auto& ids=textures[view.texture]; auto& id=ids[srgb?1:0]; if (id) return id;
            if (!view.texture->image) throw std::runtime_error("Compressed/extension-only textures are unsupported");
            auto bytes=ImageBytes(*view.texture->image,path.parent_path()); int width=0,height=0,channels=0;
            std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&width,&height,&channels,4),stbi_image_free);
            if (!pixels||width<=0||height<=0||width>16384||height>16384) throw std::runtime_error("PNG/JPEG texture decode failed");
            id=Derived(source,srgb?5:6,static_cast<std::uint64_t>(view.texture-raw->textures));
            Renderer::TextureAsset asset{id,view.texture->name?view.texture->name:"glTF texture",static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),srgb,{pixels.get(),pixels.get()+static_cast<std::size_t>(width)*height*4}};
            scene.assets.textures.push_back(std::move(asset)); return id;
        };
        Renderer::MaterialAsset fallback; fallback.id=Derived(source,2,raw->materials_count); fallback.name="Default material"; scene.assets.materials.push_back(fallback);
        for (std::size_t i=0;i<raw->materials_count;++i) {
            const auto& material=raw->materials[i];
            if (material.alpha_mode==cgltf_alpha_mode_blend||material.has_transmission||material.unlit) return Result::Failure("Blended, transmissive or unlit material requires an unsupported render path");
            Renderer::MaterialAsset asset; asset.id=Derived(source,2,i); asset.name=material.name?material.name:"Material "+std::to_string(i);
            const auto& pbr=material.pbr_metallic_roughness;
            std::copy_n(pbr.base_color_factor,4,asset.baseColor.begin()); asset.metallic=pbr.metallic_factor; asset.roughness=pbr.roughness_factor;
            asset.baseColorTexture=texture(pbr.base_color_texture,true); asset.metallicRoughnessTexture=texture(pbr.metallic_roughness_texture,false);
            asset.normalTexture=texture(material.normal_texture,false); asset.normalScale=material.normal_texture.scale;
            asset.occlusionTexture=texture(material.occlusion_texture,false); asset.occlusionStrength=material.occlusion_texture.scale;
            asset.emissiveTexture=texture(material.emissive_texture,true); asset.emissive={material.emissive_factor[0],material.emissive_factor[1],material.emissive_factor[2]};
            asset.alphaMode=material.alpha_mode==cgltf_alpha_mode_mask?1u:0u; asset.alphaCutoff=material.alpha_cutoff; asset.doubleSided=material.double_sided!=0;
            scene.assets.materials.push_back(std::move(asset));
        }
        std::vector<std::vector<PrimitiveRef>> meshRefs(raw->meshes_count); std::uint64_t primitiveIndex=0;
        for (std::size_t m=0;m<raw->meshes_count;++m) for (std::size_t p=0;p<raw->meshes[m].primitives_count;++p) {
            const auto& primitive=raw->meshes[m].primitives[p];
            if (primitive.type!=cgltf_primitive_type_triangles||primitive.has_draco_mesh_compression||primitive.targets_count) return Result::Failure("Only uncompressed static triangle primitives are supported");
            const cgltf_accessor *positions=nullptr,*normals=nullptr,*uvs=nullptr;
            for (std::size_t a=0;a<primitive.attributes_count;++a) {
                const auto& attribute=primitive.attributes[a];
                if (attribute.type==cgltf_attribute_type_position) positions=attribute.data;
                if (attribute.type==cgltf_attribute_type_normal) normals=attribute.data;
                if (attribute.type==cgltf_attribute_type_texcoord&&attribute.index==0) uvs=attribute.data;
            }
            if (!positions||positions->count>50000000) return Result::Failure("Mesh is missing positions or exceeds import limits");
            Renderer::MeshAsset mesh; mesh.id=Derived(source,1,primitiveIndex++); mesh.name=(raw->meshes[m].name?raw->meshes[m].name:"Mesh")+std::string(" / ")+std::to_string(p);
            mesh.vertices.resize(positions->count);
            for (std::size_t v=0;v<positions->count;++v) {
                float position[3]{},normal[3]{0,0,0},uv[2]{};
                if (!cgltf_accessor_read_float(positions,v,position,3)||(normals&&!cgltf_accessor_read_float(normals,v,normal,3))||(uvs&&!cgltf_accessor_read_float(uvs,v,uv,2))) return Result::Failure("Cannot unpack glTF vertex accessor");
                mesh.vertices[v]={position[0],position[1],position[2],uv[0],uv[1],normal[0],normal[1],normal[2]};
            }
            const auto count=primitive.indices?primitive.indices->count:positions->count;
            if (count%3) return Result::Failure("Triangle index count is not divisible by three");
            mesh.indices.reserve(count);
            for (std::size_t i=0;i<count;++i) { auto index=primitive.indices?cgltf_accessor_read_index(primitive.indices,i):i; if (index>=mesh.vertices.size()) return Result::Failure("Mesh index outside vertex range"); mesh.indices.push_back(static_cast<std::uint32_t>(index)); }
            if (!normals) for (std::size_t i=0;i<count;i+=3) {
                auto& a=mesh.vertices[mesh.indices[i]]; auto& b=mesh.vertices[mesh.indices[i+1]]; auto& c=mesh.vertices[mesh.indices[i+2]];
                float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
                float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
                for (auto index:{mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]}) { mesh.vertices[index].nx+=nx; mesh.vertices[index].ny+=ny; mesh.vertices[index].nz+=nz; }
            }
            for (auto& vertex:mesh.vertices) { float length=std::sqrt(vertex.nx*vertex.nx+vertex.ny*vertex.ny+vertex.nz*vertex.nz); if (length>0) { vertex.nx/=length;vertex.ny/=length;vertex.nz/=length; } else vertex.ny=1; }
            auto material=primitive.material?Derived(source,2,static_cast<std::uint64_t>(primitive.material-raw->materials)):fallback.id;
            meshRefs[m].push_back({mesh.id,material}); scene.assets.meshes.push_back(std::move(mesh));
        }
        const cgltf_scene* active=raw->scene?raw->scene:(raw->scenes_count?&raw->scenes[0]:nullptr);
        std::function<void(cgltf_node*,Guid)> visit=[&](cgltf_node* node,Guid parent) {
            ImportedNode imported; imported.id=Derived(source,3,static_cast<std::uint64_t>(node-raw->nodes)); imported.parent=parent;
            imported.name=node->name?node->name:"Node"; cgltf_node_transform_local(node,imported.local.values.data());
            if (node->mesh) imported.primitives=meshRefs[static_cast<std::size_t>(node->mesh-raw->meshes)];
            auto id=imported.id; scene.nodes.push_back(std::move(imported)); for (std::size_t c=0;c<node->children_count;++c) visit(node->children[c],id);
        };
        if (active) for (std::size_t i=0;i<active->nodes_count;++i) visit(active->nodes[i],{});
        else for (std::size_t i=0;i<raw->nodes_count;++i) if (!raw->nodes[i].parent) visit(&raw->nodes[i],{});
        if (scene.assets.meshes.empty()||scene.nodes.empty()) return Result::Failure("glTF contains no static scene geometry");
        output=std::move(scene); return Result::Success();
    } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}
