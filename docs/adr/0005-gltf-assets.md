# ADR 0005: Static glTF assets, cooking and PBR batches

Status: Accepted for M4.

Assets is a sidecar static library above Scene/Renderer. cgltf 1.15 handles glTF/GLB
parsing and accessor unpacking; pinned stb_image decodes PNG/JPEG. Imported resources
are engine-owned CPU mesh, texture and material values. AssetsModule can own a
persistent registry; no importer or cgltf types appear in renderer APIs.

The catalog gives each imported source a random GUID. Paths are source locators;
scene/resource references are GUIDs. Reimport keeps the source identity and derives
primitive/material/node/texture GUIDs by domain and source index. Cooked CBOR stores
versioned metadata, raw vertex/index buffers and RGBA texels. Cooked scenes load by
GUID without reopening the source. The current version targets little-endian native
float meshes; portable cooked format conversion and streaming are later work.

Node matrices and parent relationships are retained. Instantiation decomposes static
TRS and creates hierarchy/primitive entities through Commands. Separate primitives
become separate mesh assets, with one material each. Sparse/normalized/interleaved
accessors use cgltf decoding; missing normals are generated. UV0, embedded image
data, external PNG/JPEG and GLB buffer-view images are supported.

Base-color/emissive images are sRGB; metallic/roughness, normal and AO images are
linear. The PBR shader implements GGX distribution, Schlick Fresnel, Smith geometry,
metallic diffuse reduction, normal mapping, occlusion, emission and alpha masking.
Lighting remains direct directional/point plus a small ambient term and output
compression; no environment IBL or shadows are claimed. Blending, skins, animation,
morphs, compressed geometry, texture transforms and alternate UV sets are rejected
explicitly. Texture filtering currently uses the renderer's common linear/repeat
sampler; per-glTF sampler fidelity is a later extension.

The forward pipeline registers uploaded mesh/material GUIDs and batches instances
by both IDs. Each batch offsets its device-address instance pointer, because Slang's
HLSL SV_InstanceID excludes the first-instance offset. Material data is a GPU buffer;
indexed images and samplers expand to 256 slots. The tested NVIDIA device supports
this capacity. The CPU importer can store larger scenes, but GPU upload reports a
capacity error rather than silently dropping resources. In-session uploads skip
existing IDs; replacing a reimported GPU asset requires recreating the pipeline.

The bundled material gallery is an engine-generated reproducible multi-primitive
scene with external buffers and color/metallic-roughness textures. Optional Khronos
MetalRoughSpheresNoTextures is downloaded with commit identity and upstream license
metadata. No editor code is introduced until M4 acceptance.
