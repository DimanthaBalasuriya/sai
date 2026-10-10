"""Adapt the pinned private Vulkan backend without modifying fetched source.

Only the sRGB fragment shader selection is patched. Fail closed on vendor drift.
"""
import pathlib, struct, sys
source, spirv, output = map(pathlib.Path, sys.argv[1:])
code=source.read_text(encoding='utf-8')
data=spirv.read_bytes()
words=struct.unpack('<'+'I'*(len(data)//4),data)
array='static const uint32_t EngineGuiSrgbShader[] = {'+','.join(hex(w) for w in words)+'};\n'
needle='static void ImGui_ImplVulkan_CreateShaderModules(VkDevice device, const VkAllocationCallbacks* allocator)'
assert code.count(needle)==1
code=code.replace(needle,array+needle)
before='    // Create the shader modules\n    ImGui_ImplVulkan_Data* bd = ImGui_ImplVulkan_GetBackendData();'
after=before+'''\n    const auto& rendering = bd->VulkanInitInfo.PipelineRenderingCreateInfo;
    const bool EngineGuiSrgb = bd->VulkanInitInfo.UseDynamicRendering && rendering.colorAttachmentCount > 0 &&
        (rendering.pColorAttachmentFormats[0] == VK_FORMAT_B8G8R8A8_SRGB || rendering.pColorAttachmentFormats[0] == VK_FORMAT_R8G8B8A8_SRGB);'''
assert code.count(before)==1
code=code.replace(before,after)
for before,after in [('frag_info.codeSize = sizeof(__glsl_shader_frag_spv);','frag_info.codeSize = EngineGuiSrgb ? sizeof(EngineGuiSrgbShader) : sizeof(__glsl_shader_frag_spv);'),('frag_info.pCode = (uint32_t*)__glsl_shader_frag_spv;','frag_info.pCode = EngineGuiSrgb ? EngineGuiSrgbShader : __glsl_shader_frag_spv;')]:
 assert code.count(before)==1
 code=code.replace(before,after)
output.write_text(code,encoding='utf-8')
