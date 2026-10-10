#include "engine/rhi/Device.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Profile.hpp"
#include "engine/platform/Window.hpp"
#include <volk.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>
#include "../ResourcePool.hpp"
#include <cstring>
#include <algorithm>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <utility>
#include <array>
#include <atomic>
#include <exception>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

namespace Engine::Rhi {
namespace {

Result Check(VkResult result, const char* operation) {
    return result == VK_SUCCESS ? Result::Success()
        : Result::Failure(std::string(operation) + " failed (" + std::to_string(result) + ")");
}
}

namespace {
VkFormat NativeFormat(Format format, VkFormat swapchainFormat) {
    switch (format) {
    case Format::Rgba8Srgb: return VK_FORMAT_R8G8B8A8_SRGB;
    case Format::Bgra8Srgb: return VK_FORMAT_B8G8R8A8_SRGB;
    case Format::Rgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::Bgra8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::Rgba16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::D32Float: return VK_FORMAT_D32_SFLOAT;
    case Format::Swapchain: return swapchainFormat;
    }
    return VK_FORMAT_UNDEFINED;
}
VkPipelineStageFlags2 Stages(ResourceState state) {
    switch (state) {
    case ResourceState::HostWrite: return VK_PIPELINE_STAGE_2_HOST_BIT;
    case ResourceState::StorageRead: case ResourceState::StorageReadWrite:return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    case ResourceState::TransferSource: case ResourceState::TransferDestination: return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    case ResourceState::VertexRead: return VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    case ResourceState::IndexRead: return VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    case ResourceState::SampledRead: return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    case ResourceState::ColorWrite: return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    case ResourceState::DepthWrite: return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    default: return VK_PIPELINE_STAGE_2_NONE;
    }
}
VkAccessFlags2 Access(ResourceState state) {
    switch (state) {
    case ResourceState::HostWrite: return VK_ACCESS_2_HOST_WRITE_BIT;
    case ResourceState::StorageRead: return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    case ResourceState::StorageReadWrite:return VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    case ResourceState::TransferSource: return VK_ACCESS_2_TRANSFER_READ_BIT;
    case ResourceState::TransferDestination: return VK_ACCESS_2_TRANSFER_WRITE_BIT;
    case ResourceState::VertexRead: return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    case ResourceState::IndexRead: return VK_ACCESS_2_INDEX_READ_BIT;
    case ResourceState::SampledRead: return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    case ResourceState::ColorWrite: return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    case ResourceState::DepthWrite: return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    default: return VK_ACCESS_2_NONE;
    }
}
VkImageLayout Layout(ResourceState state) {
    switch (state) {
    case ResourceState::TransferSource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ResourceState::TransferDestination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ResourceState::SampledRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ResourceState::StorageReadWrite:return VK_IMAGE_LAYOUT_GENERAL;
    case ResourceState::ColorWrite: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ResourceState::DepthWrite: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case ResourceState::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default: return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}
struct BufferRecord {
    VmaAllocator allocator = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    BufferDesc desc;
    ResourceState state = ResourceState::Undefined;
    ~BufferRecord() { if (buffer) vmaDestroyBuffer(allocator, buffer, allocation); }
};
struct ImageRecord {
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = nullptr;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkImageView storageView=VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    ImageDesc desc;
    ResourceState state = ResourceState::Undefined;
    bool external = false;
    ~ImageRecord() {
        if (view) vkDestroyImageView(device, view, nullptr);
        if(storageView)vkDestroyImageView(device,storageView,nullptr);
        if (allocation) vmaDestroyImage(allocator, image, allocation);
    }
};
struct SamplerRecord {
    VkDevice device = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    ~SamplerRecord() { if (sampler) vkDestroySampler(device, sampler, nullptr); }
};
struct PipelineRecord {
    VkDevice device = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    PipelineDesc desc;
    bool compute=false;
    ~PipelineRecord() {
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        if (layout) vkDestroyPipelineLayout(device, layout, nullptr);
    }
};
struct TableRecord {
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    std::array<ImageHandle, kTextureTableCapacity+16> images{};
    std::array<SamplerHandle, kTextureTableCapacity> samplers{};
    std::array<ImageHandle,8> storage{};
    std::uint64_t lastSerial=UINT64_MAX;
    std::size_t lastSlot=0;
    ~TableRecord() { if (pool) vkDestroyDescriptorPool(device, pool, nullptr); }
};
template<class Function> Result Guard(Function&& function) {
    try { return function(); } catch (const std::exception& error) { return Result::Failure(error.what()); }
}
}
struct Device::Impl : CommandList {
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE; VkCommandBuffer commands = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE; VkFence fence = VK_NULL_HANDLE;
        VkQueryPool timestamps = VK_NULL_HANDLE; bool submitted = false;
        std::vector<std::string> passNames;
    };
    Window* window = nullptr;
    vkb::Instance instance{}; vkb::Device device{}; vkb::Swapchain swapchain{};
    VkSurfaceKHR surface = VK_NULL_HANDLE; VkQueue graphicsQueue = VK_NULL_HANDLE, presentQueue = VK_NULL_HANDLE;
    std::uint32_t graphicsFamily = 0; VmaAllocator allocator = nullptr;
    VkDescriptorSetLayout textureLayout = VK_NULL_HANDLE;
    VkDescriptorPool guiPool=VK_NULL_HANDLE;
    VkSampler guiSampler=VK_NULL_HANDLE;
    bool guiReady=false;
    VkFormat guiFormat=VK_FORMAT_UNDEFINED;
    ResourcePool<BufferRecord, BufferHandle> buffers;
    ResourcePool<ImageRecord, ImageHandle> imageResources;
    ResourcePool<SamplerRecord, SamplerHandle> samplers;
    ResourcePool<PipelineRecord, PipelineHandle> pipelines;
    ResourcePool<TableRecord, TextureTableHandle> tables;
    std::array<Frame, kFramesInFlight> frames{};
    std::vector<VkImage> images; std::vector<VkImageView> views;
    std::vector<ImageHandle> swapchainHandles; std::vector<VkSemaphore> renderFinished;
    std::vector<PassTiming> passTimings;
    std::atomic<std::uint32_t> validationMessages{0};
    std::uint64_t presentedFrames = 0; std::uint32_t generations = 0, currentImage = 0;
    std::size_t frameIndex = 0; int requestedWidth = 0, requestedHeight = 0;
    std::uint64_t frameSerial=0;
    bool recreate = false, ready = false, active = false, renderingActive = false, passActive = false;
    bool meshBound = false, tableBound = false, constantsBound = false;
    PipelineHandle boundPipeline{};
    double timestampPeriod = 0.0, gpuFrameMilliseconds = 0.0; std::uint32_t timestampBits = 0;bool driverMemoryBudget=false;
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) noexcept {
        auto& self = *static_cast<Impl*>(user);
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            ++self.validationMessages;
            Log::Error(data->pMessage);
        } else {
            Log::Info(data->pMessage);
        }
        return VK_FALSE;
    }

    template<typename Handle>
    void Name(VkObjectType type, Handle handle, const char* name) const noexcept {
        if (!vkSetDebugUtilsObjectNameEXT || !handle) return;
        VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        info.objectType = type;
        // Vulkan handles are pointers on x64 and integers on some other platforms.
        if constexpr (std::is_pointer_v<Handle>) info.objectHandle = reinterpret_cast<std::uint64_t>(handle);
        else info.objectHandle = static_cast<std::uint64_t>(handle);
        info.pObjectName = name;
        if (vkSetDebugUtilsObjectNameEXT(device.device, &info) != VK_SUCCESS) Log::Error("Vulkan object naming failed");
    }

    AdapterInfo adapter;
    Result Initialize(Window& target,std::string_view gpuOverride) {
        if (instance.instance) return Result::Failure("Device initialization may only be attempted once");
        window = &target;
        auto result = Check(volkInitialize(), "Load Vulkan");
        if (!result) return result;
        std::uint32_t extensionCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (!extensions) return Result::Failure("GLFW reports no Vulkan surface support");
        vkb::InstanceBuilder builder(vkGetInstanceProcAddr);
        builder.set_app_name("Engine").require_api_version(1, 3, 0)
            .enable_extensions(extensionCount, extensions)
            .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
            .set_debug_callback(DebugCallback).set_debug_callback_user_data_pointer(this)
            .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
#ifndef NDEBUG
        builder.enable_validation_layers()
            .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
#endif
        auto instanceResult = builder.build();
        if (!instanceResult) return Result::Failure("Vulkan instance: " + instanceResult.error().message());
        instance = instanceResult.value();
        volkLoadInstance(instance.instance);
        result = Check(glfwCreateWindowSurface(instance.instance,
            static_cast<GLFWwindow*>(window->NativeHandle()), nullptr, &surface), "Create window surface");
        if (!result) return result;
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features12.bufferDeviceAddress = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        // Slang implements HLSL SV_VertexID using VertexIndex and BaseVertex.
        features11.shaderDrawParameters = VK_TRUE;
        VkPhysicalDeviceFeatures features{};
        features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
        features.shaderStorageImageArrayDynamicIndexing=VK_TRUE;
        features.shaderStorageImageExtendedFormats=VK_TRUE;features.shaderStorageImageReadWithoutFormat=VK_TRUE;features.shaderStorageImageWriteWithoutFormat=VK_TRUE;
        features.fillModeNonSolid=VK_TRUE;
        auto compatible = vkb::PhysicalDeviceSelector(instance).set_surface(surface).add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
            .allow_any_gpu_device_type(true).set_required_features(features).set_minimum_version(1,3)
            .set_required_features_11(features11).set_required_features_12(features12).set_required_features_13(features13).select_devices();
        std::uint32_t deviceCount=0;
        result=Check(vkEnumeratePhysicalDevices(instance.instance,&deviceCount,nullptr),"Enumerate GPU count");if(!result)return result;
        std::vector<VkPhysicalDevice> handles(deviceCount);
        result=Check(vkEnumeratePhysicalDevices(instance.instance,&deviceCount,handles.data()),"Enumerate GPUs");if(!result)return result;
        handles.resize(deviceCount);std::vector<AdapterInfo> candidates;
        for(std::uint32_t i=0;i<handles.size();++i){
            VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(handles[i],&properties);
            VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(handles[i],&memory);
            AdapterInfo info;info.index=i;info.name=properties.deviceName;info.driverVersion=properties.driverVersion;
            switch(properties.deviceType){
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:info.type=AdapterClass::Discrete;break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:info.type=AdapterClass::Integrated;break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:info.type=AdapterClass::Virtual;break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:info.type=AdapterClass::Software;break;
            default:info.type=AdapterClass::Other;break;
            }
            for(std::uint32_t h=0;h<memory.memoryHeapCount;++h)if(memory.memoryHeaps[h].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)info.localMemoryBytes+=memory.memoryHeaps[h].size;
            bool supported=compatible&&std::any_of(compatible.value().begin(),compatible.value().end(),[&](const auto& d){return d.physical_device==handles[i];});
            if(!supported){
                VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
                VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};f12.pNext=&f13;
                VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};f.pNext=&f12;vkGetPhysicalDeviceFeatures2(handles[i],&f);
                if(properties.apiVersion<VK_API_VERSION_1_3)info.missing+="Vulkan 1.3; ";
                if(!f13.dynamicRendering)info.missing+="dynamic rendering; ";
                if(!f13.synchronization2)info.missing+="synchronization2; ";
                if(!f12.bufferDeviceAddress)info.missing+="buffer device address; ";
                if(!f12.descriptorIndexing||!f12.runtimeDescriptorArray||!f12.descriptorBindingPartiallyBound||!f12.descriptorBindingVariableDescriptorCount||!f12.shaderSampledImageArrayNonUniformIndexing)info.missing+="descriptor indexing features; ";
                if(!f.features.fillModeNonSolid)info.missing+="wireframe; ";
                if(!f.features.shaderStorageImageReadWithoutFormat||!f.features.shaderStorageImageWriteWithoutFormat)info.missing+="formatless storage images; ";
                if(info.missing.empty())info.missing="required queues, extensions or shader features unavailable";
            }
            Log::Info("GPU ["+std::to_string(i)+"] "+info.name+" | local MiB: "+std::to_string(info.localMemoryBytes/(1024*1024))+" | "+(info.missing.empty()?"compatible":info.missing));
            candidates.push_back(std::move(info));
        }
        std::size_t selected;result=SelectAdapter(candidates,gpuOverride,selected);if(!result)return result;
        adapter=candidates[selected];
        auto chosen=std::find_if(compatible.value().begin(),compatible.value().end(),[&](const auto& d){return d.physical_device==handles[selected];});
        bool driverBudget=chosen->enable_extension_if_present(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);driverMemoryBudget=driverBudget;auto deviceResult = vkb::DeviceBuilder(*chosen).build();
        if (!deviceResult) return Result::Failure("Vulkan device: " + deviceResult.error().message());
        device = deviceResult.value();
        volkLoadDevice(device.device);
        auto graphics = device.get_queue(vkb::QueueType::graphics);
        auto present = device.get_queue(vkb::QueueType::present);
        auto family = device.get_queue_index(vkb::QueueType::graphics);
        if (!graphics || !present || !family) return Result::Failure("Graphics/present queues unavailable");
        graphicsQueue = graphics.value();
        presentQueue = present.value();
        graphicsFamily = family.value();
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device.physical_device, &properties);
        timestampPeriod = properties.limits.timestampPeriod;
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device, &queueFamilyCount, queueFamilies.data());
        timestampBits = queueFamilies[graphicsFamily].timestampValidBits;
        if (timestampBits == 0) return Result::Failure("Graphics queue lacks timestamps required for debug timing");
        Name(VK_OBJECT_TYPE_DEVICE, device.device, "Engine device");
        Name(VK_OBJECT_TYPE_INSTANCE, instance.instance, "Engine instance");
        Name(VK_OBJECT_TYPE_PHYSICAL_DEVICE, device.physical_device.physical_device, "Selected GPU");
        Name(VK_OBJECT_TYPE_SURFACE_KHR, surface, "Main window surface");
        // The debug messenger is owned by the loader, not a device driver object.
        // Passing its handle to the device naming entry point crashes some drivers.
        Name(VK_OBJECT_TYPE_QUEUE, graphicsQueue, "Graphics queue");
        if (presentQueue != graphicsQueue) Name(VK_OBJECT_TYPE_QUEUE, presentQueue, "Present queue");
        adapter.graphicsFamily=graphicsFamily;adapter.presentFamily=device.get_queue_index(vkb::QueueType::present).value();
        Log::Info("Selected GPU ["+std::to_string(adapter.index)+"] "+adapter.name+" | driver "+std::to_string(adapter.driverVersion)+" | local MiB "+std::to_string(adapter.localMemoryBytes/(1024*1024))+" | graphics/present queues "+std::to_string(adapter.graphicsFamily)+"/"+std::to_string(adapter.presentFamily));
        VmaVulkanFunctions functions{};
        functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        VmaAllocatorCreateInfo allocationInfo{};
        allocationInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT|(driverBudget?VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT:0);
        allocationInfo.instance = instance.instance;
        allocationInfo.physicalDevice = device.physical_device;
        allocationInfo.device = device.device;
        allocationInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocationInfo.pVulkanFunctions = &functions;
        result = Check(vmaCreateAllocator(&allocationInfo, &allocator), "Create VMA allocator");
        if (!result) return result;
        Log::Info("VMA allocator initialized");
        result = CreateDescriptorLayout();
        if (!result) return result;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            auto& frame = frames[i];
            VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = graphicsFamily;
            result = Check(vkCreateCommandPool(device.device, &poolInfo, nullptr, &frame.pool), "Create command pool");
            if (!result) return result;
            VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            commandInfo.commandPool = frame.pool;
            commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandInfo.commandBufferCount = 1;
            result = Check(vkAllocateCommandBuffers(device.device, &commandInfo, &frame.commands), "Allocate command buffer");
            if (!result) return result;
            VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            result = Check(vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &frame.acquired), "Create acquire semaphore");
            if (!result) return result;
            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            result = Check(vkCreateFence(device.device, &fenceInfo, nullptr, &frame.fence), "Create frame fence");
            if (!result) return result;
            VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryInfo.queryCount = 2 + 2 * kMaxTimedPasses;
            result = Check(vkCreateQueryPool(device.device, &queryInfo, nullptr, &frame.timestamps), "Create timestamp pool");
            if (!result) return result;
            const auto suffix = " " + std::to_string(i);
            Name(VK_OBJECT_TYPE_COMMAND_POOL, frame.pool, ("Frame command pool" + suffix).c_str());
            Name(VK_OBJECT_TYPE_COMMAND_BUFFER, frame.commands, ("Frame command buffer" + suffix).c_str());
            Name(VK_OBJECT_TYPE_SEMAPHORE, frame.acquired, ("Image acquired" + suffix).c_str());
            Name(VK_OBJECT_TYPE_FENCE, frame.fence, ("Frame completed" + suffix).c_str());
            Name(VK_OBJECT_TYPE_QUERY_POOL, frame.timestamps, ("Triangle timestamps" + suffix).c_str());
        }
        ready = true;
        Log::Info("Frame synchronization initialized");
        return RecreateSwapchain();
    }

    Result LoadShader(const std::filesystem::path& path, const char* name, VkShaderModule& shader) {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream) return Result::Failure("Cannot open shader: " + path.string());
        const auto size = stream.tellg();
        if (size <= 0 || static_cast<std::uint64_t>(size) % sizeof(std::uint32_t) != 0)
            return Result::Failure("Invalid SPIR-V file: " + path.string());
        std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(words.data()), size)) return Result::Failure("Failed reading shader");
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = static_cast<std::size_t>(size);
        info.pCode = words.data();
        auto result = Check(vkCreateShaderModule(device.device, &info, nullptr, &shader), "Create shader module");
        if (result) Name(VK_OBJECT_TYPE_SHADER_MODULE, shader, name);
        return result;
    }

    Result CreateDescriptorLayout() {
        std::array<VkDescriptorSetLayoutBinding,7> bindings{};
        for(std::uint32_t i=0;i<bindings.size();++i){bool storage=i==2||i==5||i==6;bindings[i]={i,i==1?VK_DESCRIPTOR_TYPE_SAMPLER:storage?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,i<2?kTextureTableCapacity:8,VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_COMPUTE_BIT,nullptr};}
        std::array<VkDescriptorBindingFlags,7> flags{};flags.fill(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        VkDescriptorSetLayoutBindingFlagsCreateInfo indexing{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        indexing.bindingCount = static_cast<std::uint32_t>(bindings.size()); indexing.pBindingFlags = flags.data();
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.pNext = &indexing; info.bindingCount = static_cast<std::uint32_t>(bindings.size()); info.pBindings = bindings.data();
        auto result = Check(vkCreateDescriptorSetLayout(device.device, &info, nullptr, &textureLayout), "Create indexed texture layout");
        if (result) Name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, textureLayout, "Indexed textures and samplers");
        return result;
    }
    Result CreateBuffer(const BufferDesc& desc, BufferHandle& output) {
        output = {};
        if (!ready || desc.bytes == 0 || desc.name.empty()) return Result::Failure("Invalid buffer description/device");
        auto record = std::make_unique<BufferRecord>();
        record->allocator = allocator; record->desc = desc;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = desc.bytes;
        if (HasFlag(desc.usage, BufferUsage::Vertex)) info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (HasFlag(desc.usage, BufferUsage::Index)) info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (HasFlag(desc.usage, BufferUsage::Storage)) info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (HasFlag(desc.usage, BufferUsage::Uniform)) info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (HasFlag(desc.usage, BufferUsage::TransferSource)) info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (HasFlag(desc.usage, BufferUsage::TransferDestination)) info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (HasFlag(desc.usage, BufferUsage::DeviceAddress)) info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        if (!info.usage) return Result::Failure("Buffer usage is empty");
        VmaAllocationCreateInfo memory{};
        memory.usage = VMA_MEMORY_USAGE_AUTO;
        if (desc.hostVisible) memory.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        auto result = Check(vmaCreateBuffer(allocator, &info, &memory, &record->buffer, &record->allocation, nullptr), "Create buffer");
        if (!result) return result;
        Name(VK_OBJECT_TYPE_BUFFER, record->buffer, desc.name.c_str());
        vmaSetAllocationName(allocator, record->allocation, desc.name.c_str());
        output = buffers.Insert(std::move(record));
        return Result::Success();
    }
    Result CreateImage(const ImageDesc& desc, ImageHandle& output) {
        output = {};
        if (!ready || !desc.width || !desc.height || !desc.depth||desc.name.empty() || desc.format == Format::Swapchain||(desc.dimension!=ImageDimension::Texture3D&&desc.depth!=1)||(desc.dimension==ImageDimension::Cube&&desc.width!=desc.height))
            return Result::Failure("Invalid image description/device");
        auto record = std::make_unique<ImageRecord>();
        record->device = device.device; record->allocator = allocator; record->desc = desc;
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = desc.dimension==ImageDimension::Texture3D?VK_IMAGE_TYPE_3D:VK_IMAGE_TYPE_2D; info.extent = {desc.width, desc.height, desc.depth};
        info.format = NativeFormat(desc.format, swapchain.image_format);
        info.mipLevels = 1; info.arrayLayers = desc.dimension==ImageDimension::Cube?6u:1u; info.samples = VK_SAMPLE_COUNT_1_BIT;if(desc.dimension==ImageDimension::Cube)info.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        if (HasFlag(desc.usage, ImageUsage::Sampled)) info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (HasFlag(desc.usage, ImageUsage::ColorAttachment)) info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (HasFlag(desc.usage, ImageUsage::DepthAttachment)) info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (HasFlag(desc.usage, ImageUsage::TransferSource)) info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (HasFlag(desc.usage, ImageUsage::TransferDestination)) info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if(HasFlag(desc.usage,ImageUsage::Storage))info.usage|=VK_IMAGE_USAGE_STORAGE_BIT;
        if (!info.usage || (desc.format == Format::D32Float) != HasFlag(desc.usage, ImageUsage::DepthAttachment))
            return Result::Failure("Image format/usage mismatch");
        VkImageFormatProperties supported{};
        auto result = Check(vkGetPhysicalDeviceImageFormatProperties(device.physical_device, info.format,
            info.imageType, info.tiling, info.usage, info.flags, &supported), "Check image format support");
        if (!result) return result;
        if (desc.width > supported.maxExtent.width || desc.height > supported.maxExtent.height||desc.depth>supported.maxExtent.depth||info.arrayLayers>supported.maxArrayLayers) return Result::Failure("Image extent exceeds device limits");
        VmaAllocationCreateInfo memory{}; memory.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        result = Check(vmaCreateImage(allocator, &info, &memory, &record->image, &record->allocation, nullptr), "Create image");
        if (!result) return result;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = record->image; viewInfo.viewType = desc.dimension==ImageDimension::Texture3D?VK_IMAGE_VIEW_TYPE_3D:desc.dimension==ImageDimension::Cube?VK_IMAGE_VIEW_TYPE_CUBE:VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = info.format;
        viewInfo.subresourceRange = {static_cast<VkImageAspectFlags>(desc.format == Format::D32Float ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 1, 0, info.arrayLayers};
        result = Check(vkCreateImageView(device.device, &viewInfo, nullptr, &record->view), "Create image view");
        if (!result) return result;
        if(desc.dimension==ImageDimension::Cube&&HasFlag(desc.usage,ImageUsage::Storage)){viewInfo.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY;result=Check(vkCreateImageView(device.device,&viewInfo,nullptr,&record->storageView),"Create cube storage view");if(!result)return result;}
        Name(VK_OBJECT_TYPE_IMAGE, record->image, desc.name.c_str());
        Name(VK_OBJECT_TYPE_IMAGE_VIEW, record->view, (desc.name + " view").c_str());
        vmaSetAllocationName(allocator, record->allocation, desc.name.c_str());
        output = imageResources.Insert(std::move(record));
        return Result::Success();
    }
    Result CreateSampler(const SamplerDesc& desc, SamplerHandle& output) {
        output = {};
        if (!ready || desc.name.empty()) return Result::Failure("Invalid sampler description/device");
        auto record = std::make_unique<SamplerRecord>(); record->device = device.device;
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        info.magFilter = info.minFilter = desc.linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = info.addressModeV = info.addressModeW = desc.repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        auto result = Check(vkCreateSampler(device.device, &info, nullptr, &record->sampler), "Create sampler");
        if (!result) return result;
        Name(VK_OBJECT_TYPE_SAMPLER, record->sampler, desc.name.c_str());
        output = samplers.Insert(std::move(record)); return Result::Success();
    }
    Result CreateTextureTable(std::string_view name, TextureTableHandle& output) {
        output = {};
        if (!ready || name.empty()) return Result::Failure("Invalid texture table/device");
        auto record = std::make_unique<TableRecord>(); record->device = device.device;
        std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,kTextureTableCapacity+16},{VK_DESCRIPTOR_TYPE_SAMPLER,kTextureTableCapacity},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,24}}};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets = 1; info.poolSizeCount = static_cast<std::uint32_t>(sizes.size()); info.pPoolSizes = sizes.data();
        auto result = Check(vkCreateDescriptorPool(device.device, &info, nullptr, &record->pool), "Create descriptor pool");
        if (!result) return result;
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool = record->pool; allocate.descriptorSetCount = 1; allocate.pSetLayouts = &textureLayout;
        result = Check(vkAllocateDescriptorSets(device.device, &allocate, &record->set), "Allocate texture descriptor table");
        if (!result) return result;
        Name(VK_OBJECT_TYPE_DESCRIPTOR_POOL, record->pool, (std::string(name) + " pool").c_str());
        Name(VK_OBJECT_TYPE_DESCRIPTOR_SET, record->set, std::string(name).c_str());
        output = tables.Insert(std::move(record)); return Result::Success();
    }
    Result SetTexture(TextureTableHandle tableHandle, std::uint32_t index, ImageHandle imageHandle, SamplerHandle samplerHandle,bool frameWrite=false) {
        auto* table = tables.Get(tableHandle); auto* image = imageResources.Get(imageHandle); auto* sampler = samplers.Get(samplerHandle);
        if (active != frameWrite || !table || !image || !sampler || index >= kTextureTableCapacity || !HasFlag(image->desc.usage, ImageUsage::Sampled))
            return Result::Failure("Invalid texture descriptor write");
        if(image->desc.dimension!=ImageDimension::Texture2D&&index>=8)return Result::Failure("Volume/cube texture index exceeds capacity");
        if (frameWrite&&table->lastSerial!=UINT64_MAX&&(table->lastSerial==frameSerial||table->lastSlot!=frameIndex))
            return Result::Failure("Frame texture table must belong to the current fenced slot and be updated before binding");
        if (!frameWrite) { auto result = Check(vkDeviceWaitIdle(device.device), "Wait before descriptor update"); if (!result) return result; }
        VkDescriptorImageInfo imageInfo{}; imageInfo.imageView = image->view; imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo samplerInfo{}; samplerInfo.sampler = sampler->sampler;
        std::array<VkWriteDescriptorSet, 2> writes{};
        for (auto& write : writes) { write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; write.dstSet = table->set; write.dstArrayElement = index; write.descriptorCount = 1; }
        writes[0].dstBinding = image->desc.dimension==ImageDimension::Texture3D?3u:image->desc.dimension==ImageDimension::Cube?4u:0u; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; writes[0].pImageInfo = &imageInfo;
        writes[1].dstBinding = 1; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; writes[1].pImageInfo = &samplerInfo;
        vkUpdateDescriptorSets(device.device, 2, writes.data(), 0, nullptr);
        table->images[index+(image->desc.dimension==ImageDimension::Texture3D?256:image->desc.dimension==ImageDimension::Cube?264:0)] = imageHandle; table->samplers[index] = samplerHandle;
        return Result::Success();
    }
    Result WriteStorageImage(TextureTableHandle handle,std::uint32_t index,ImageHandle imageHandle){auto table=tables.Get(handle);auto image=imageResources.Get(imageHandle);if(!active||!table||!image||index>=8||!HasFlag(image->desc.usage,ImageUsage::Storage))return Result::Failure("Invalid storage image descriptor");if(table->lastSerial!=UINT64_MAX&&(table->lastSerial==frameSerial||table->lastSlot!=frameIndex))return Result::Failure("Storage table must belong to the fenced slot and be written before binding");VkDescriptorImageInfo info{};info.imageView=image->storageView?image->storageView:image->view;info.imageLayout=VK_IMAGE_LAYOUT_GENERAL;VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=table->set;write.dstBinding=image->desc.dimension==ImageDimension::Texture3D?5u:image->desc.dimension==ImageDimension::Cube?6u:2u;write.dstArrayElement=index;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;write.pImageInfo=&info;vkUpdateDescriptorSets(device.device,1,&write,0,nullptr);table->storage[index]=imageHandle;return Result::Success();}
    template<class Function> Result ImmediateSubmit(const char* name, Function&& record) {
        struct UploadCommands {
            VkDevice device; VkCommandPool pool = VK_NULL_HANDLE; VkFence fence = VK_NULL_HANDLE;
            ~UploadCommands() { if (fence) vkDestroyFence(device, fence, nullptr); if (pool) vkDestroyCommandPool(device, pool, nullptr); }
        } upload{device.device};
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; poolInfo.queueFamilyIndex = graphicsFamily;
        auto result = Check(vkCreateCommandPool(device.device, &poolInfo, nullptr, &upload.pool), "Create upload pool"); if (!result) return result;
        Name(VK_OBJECT_TYPE_COMMAND_POOL, upload.pool, name);
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = upload.pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 1;
        result = Check(vkAllocateCommandBuffers(device.device, &allocate, &commands), "Allocate upload commands"); if (!result) return result;
        Name(VK_OBJECT_TYPE_COMMAND_BUFFER, commands, name);
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        result = Check(vkCreateFence(device.device, &fenceInfo, nullptr, &upload.fence), "Create upload fence"); if (!result) return result;
        Name(VK_OBJECT_TYPE_FENCE, upload.fence, name);
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = Check(vkBeginCommandBuffer(commands, &begin), "Begin upload"); if (!result) return result;
        VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT}; label.pLabelName = name;
        if (vkCmdBeginDebugUtilsLabelEXT) vkCmdBeginDebugUtilsLabelEXT(commands, &label);
        record(commands);
        if (vkCmdEndDebugUtilsLabelEXT) vkCmdEndDebugUtilsLabelEXT(commands);
        result = Check(vkEndCommandBuffer(commands), "End upload"); if (!result) return result;
        VkCommandBufferSubmitInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO}; command.commandBuffer = commands;
        VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2}; submit.commandBufferInfoCount = 1; submit.pCommandBufferInfos = &command;
        result = Check(vkQueueSubmit2(graphicsQueue, 1, &submit, upload.fence), "Submit upload"); if (!result) return result;
        return Check(vkWaitForFences(device.device, 1, &upload.fence, VK_TRUE, UINT64_MAX), "Wait for staging upload");
    }
    Result FillStaging(BufferHandle handle, std::span<const std::byte> bytes,std::uint64_t offset=0) {
        auto* record = buffers.Get(handle);
        void* mapped = nullptr;
        auto result = Check(vmaMapMemory(allocator, record->allocation, &mapped), "Map staging buffer"); if (!result) return result;
        std::memcpy(static_cast<std::byte*>(mapped)+offset, bytes.data(), bytes.size());
        result = Check(vmaFlushAllocation(allocator, record->allocation, offset, bytes.size()), "Flush staging buffer");
        vmaUnmapMemory(allocator, record->allocation); return result;
    }
    Result UploadBuffer(BufferHandle handle, std::span<const std::byte> bytes, ResourceState finalState) {
        auto* destination = buffers.Get(handle);
        if (active || !destination || bytes.empty() || bytes.size() > destination->desc.bytes ||
            !HasFlag(destination->desc.usage, BufferUsage::TransferDestination) ||
            (finalState != ResourceState::VertexRead && finalState != ResourceState::IndexRead && finalState != ResourceState::TransferSource && finalState != ResourceState::StorageRead))
            return Result::Failure("Invalid buffer upload");
        if ((finalState == ResourceState::VertexRead && !HasFlag(destination->desc.usage, BufferUsage::Vertex)) ||
            (finalState == ResourceState::IndexRead && !HasFlag(destination->desc.usage, BufferUsage::Index)) ||
            (finalState == ResourceState::TransferSource && !HasFlag(destination->desc.usage, BufferUsage::TransferSource)) ||
            (finalState == ResourceState::StorageRead && !HasFlag(destination->desc.usage,BufferUsage::Storage))) return Result::Failure("Upload final state conflicts with buffer usage");
        BufferHandle staging;
        auto result = CreateBuffer({destination->desc.name + " staging", bytes.size(), BufferUsage::TransferSource, true}, staging); if (!result) return result;
        struct StagingCleanup { Impl& owner; BufferHandle handle; ~StagingCleanup() { owner.buffers.Remove(handle); } } cleanup{*this, staging};
        result = FillStaging(staging, bytes); if (!result) return result;
        result = ImmediateSubmit("Buffer staging upload", [&](VkCommandBuffer commands) {
            VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
            barrier.srcStageMask = Stages(destination->state); barrier.srcAccessMask = Access(destination->state);
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = destination->buffer; barrier.size = VK_WHOLE_SIZE;
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO}; dependency.bufferMemoryBarrierCount = 1; dependency.pBufferMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(commands, &dependency);
            VkBufferCopy copy{0, 0, bytes.size()}; vkCmdCopyBuffer(commands, buffers.Get(staging)->buffer, destination->buffer, 1, &copy);
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = Stages(finalState); barrier.dstAccessMask = Access(finalState);
            vkCmdPipelineBarrier2(commands, &dependency);
        });
        if (result) destination->state = finalState;
        return result;
    }
    Result UploadImage(ImageHandle handle, std::span<const std::byte> bytes) {
        auto* destination = imageResources.Get(handle);
        if (active || !destination || destination->external || destination->desc.format == Format::D32Float ||
            !HasFlag(destination->desc.usage, ImageUsage::TransferDestination) || !HasFlag(destination->desc.usage, ImageUsage::Sampled) ||
            bytes.size() != static_cast<std::size_t>(destination->desc.width) * destination->desc.height * destination->desc.depth*(destination->desc.dimension==ImageDimension::Cube?6:1)*(destination->desc.format==Format::Rgba16Float?8:4))
            return Result::Failure("Invalid RGBA image upload");
        BufferHandle staging;
        auto result = CreateBuffer({destination->desc.name + " staging", bytes.size(), BufferUsage::TransferSource, true}, staging); if (!result) return result;
        struct StagingCleanup { Impl& owner; BufferHandle handle; ~StagingCleanup() { owner.buffers.Remove(handle); } } cleanup{*this, staging};
        result = FillStaging(staging, bytes); if (!result) return result;
        result = ImmediateSubmit("Texture staging upload", [&](VkCommandBuffer commands) {
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            barrier.srcStageMask = Stages(destination->state); barrier.srcAccessMask = Access(destination->state);
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.oldLayout = Layout(destination->state); barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = destination->image; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, destination->desc.dimension==ImageDimension::Cube?6u:1u};
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO}; dependency.imageMemoryBarrierCount = 1; dependency.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(commands, &dependency);
            VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, destination->desc.dimension==ImageDimension::Cube?6u:1u}; copy.imageExtent = {destination->desc.width, destination->desc.height, destination->desc.depth};
            vkCmdCopyBufferToImage(commands, buffers.Get(staging)->buffer, destination->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = Stages(ResourceState::SampledRead); barrier.dstAccessMask = Access(ResourceState::SampledRead);
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier2(commands, &dependency);
        });
        if (result) destination->state = ResourceState::SampledRead;
        return result;
    }
    Result ReadImage(ImageHandle handle,std::vector<std::byte>& pixels){
        auto* image=imageResources.Get(handle);if(active||!image||image->external||image->desc.format!=Format::Rgba16Float||!HasFlag(image->desc.usage,ImageUsage::TransferSource))return Result::Failure("Diagnostic capture requires an idle RGBA16 image with transfer source usage");
        auto bytes=std::size_t(image->desc.width)*image->desc.height*8;BufferHandle staging;auto r=CreateBuffer({"Diagnostic image readback",bytes,BufferUsage::TransferDestination,true},staging);if(!r)return r;struct Cleanup{Impl& owner;BufferHandle buffer;~Cleanup(){owner.buffers.Remove(buffer);}}cleanup{*this,staging};
        r=ImmediateSubmit("Diagnostic image capture",[&](VkCommandBuffer commands){VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};barrier.srcStageMask=Stages(image->state);barrier.srcAccessMask=Access(image->state);barrier.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;barrier.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;barrier.oldLayout=Layout(image->state);barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=image->image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.imageMemoryBarrierCount=1;dependency.pImageMemoryBarriers=&barrier;vkCmdPipelineBarrier2(commands,&dependency);VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={image->desc.width,image->desc.height,1};vkCmdCopyImageToBuffer(commands,image->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffers.Get(staging)->buffer,1,&copy);barrier.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;barrier.srcAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;barrier.dstStageMask=Stages(image->state);barrier.dstAccessMask=Access(image->state);barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=Layout(image->state);vkCmdPipelineBarrier2(commands,&dependency);VkBufferMemoryBarrier2 host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};host.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;host.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;host.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;host.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=buffers.Get(staging)->buffer;host.size=VK_WHOLE_SIZE;VkDependencyInfo hostDependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};hostDependency.bufferMemoryBarrierCount=1;hostDependency.pBufferMemoryBarriers=&host;vkCmdPipelineBarrier2(commands,&hostDependency);});if(!r)return r;
        pixels.resize(bytes);auto* record=buffers.Get(staging);void* mapped=nullptr;r=Check(vmaMapMemory(allocator,record->allocation,&mapped),"Map diagnostic capture");if(r){r=Check(vmaInvalidateAllocation(allocator,record->allocation,0,bytes),"Invalidate diagnostic capture");if(r)std::memcpy(pixels.data(),mapped,bytes);vmaUnmapMemory(allocator,record->allocation);}return r;
    }
    Result BuildPipeline(PipelineRecord& record) {
        VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
        struct ShaderCleanup {
            VkDevice device;
            VkShaderModule& vertex;
            VkShaderModule& fragment;
            ~ShaderCleanup() {
                if (vertex) vkDestroyShaderModule(device, vertex, nullptr);
                if (fragment) vkDestroyShaderModule(device, fragment, nullptr);
            }
        } cleanup{device.device, vertex, fragment};
        auto result = LoadShader(record.desc.vertexShader, "Triangle vertex shader", vertex);
        if (!result) return result;
        result = LoadShader(record.desc.fragmentShader, "Triangle fragment shader", fragment);
        if (!result) return result;
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertex;
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragment;
        stages[1].pName = "main";
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkVertexInputBindingDescription binding{0, record.desc.vertexStride, VK_VERTEX_INPUT_RATE_VERTEX};
        std::vector<VkVertexInputAttributeDescription> attributes;
        for (const auto& attribute : record.desc.attributes) {
            VkFormat format = attribute.format == VertexFormat::Float2 ? VK_FORMAT_R32G32_SFLOAT
                : attribute.format == VertexFormat::Float3 ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
            attributes.push_back({attribute.location, 0, format, attribute.offset});
        }
        vertexInput.vertexBindingDescriptionCount = record.desc.vertexStride ? 1u : 0u;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = record.desc.depthTest;
        depth.depthWriteEnable = record.desc.depthTest;
        depth.depthCompareOp = record.desc.reverseZ?VK_COMPARE_OP_GREATER:VK_COMPARE_OP_LESS;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = record.desc.lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = record.desc.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;
        raster.depthBiasEnable=record.desc.depthBias!=0||record.desc.slopeBias!=0;raster.depthBiasConstantFactor=record.desc.depthBias;raster.depthBiasSlopeFactor=record.desc.slopeBias;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = record.desc.depthOnly?0u:1u;
        blend.pAttachments = &attachment;
        std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rendering.colorAttachmentCount = record.desc.depthOnly?0u:1u;
        const auto colorFormat = NativeFormat(record.desc.colorFormat, swapchain.image_format);
        rendering.pColorAttachmentFormats = &colorFormat;
        rendering.depthAttachmentFormat = record.desc.depthTest ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.pNext = &rendering;
        info.stageCount = static_cast<std::uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.pDepthStencilState = &depth;
        info.layout = record.layout;
        result = Check(vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &info, nullptr, &record.pipeline), "Create triangle pipeline");
        if (result) Name(VK_OBJECT_TYPE_PIPELINE, record.pipeline, record.desc.name.c_str());
        return result;
    }

    void DestroySwapchain() noexcept {
        for (auto semaphore : renderFinished) if (semaphore) vkDestroySemaphore(device.device, semaphore, nullptr);
        renderFinished.clear();
        for (auto handle : swapchainHandles) imageResources.Remove(handle);
        swapchainHandles.clear();
        for (auto view : views) if (view) vkDestroyImageView(device.device, view, nullptr);
        views.clear();
        images.clear();
        if (swapchain.swapchain) vkb::destroy_swapchain(swapchain);
        swapchain = {};
    }

    Result RecreateSwapchain() {
        const auto extent = window->FramebufferExtent();
        if (extent.width <= 0 || extent.height <= 0) { recreate = true; return Result::Success(); }
        auto result = Check(vkDeviceWaitIdle(device.device), "Wait before swapchain recreation");
        if (!result) return result;
        vkb::SwapchainBuilder builder(device);
        builder.set_desired_extent(static_cast<std::uint32_t>(extent.width), static_cast<std::uint32_t>(extent.height))
            .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .set_old_swapchain(swapchain.swapchain);
        auto built = builder.build();
        if (!built) return Result::Failure("Swapchain: " + built.error().message());
        const auto previousFormat = swapchain.image_format;
        DestroySwapchain();
        swapchain = built.value();
        Log::Info("Swapchain created");
        Name(VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchain.swapchain, "Main window swapchain");
        auto imageResult = swapchain.get_images();
        if (!imageResult) return Result::Failure("Cannot retrieve swapchain images");
        images = imageResult.value();
        auto viewResult = swapchain.get_image_views();
        if (!viewResult) return Result::Failure("Cannot create swapchain image views");
        views = viewResult.value();

        renderFinished.resize(images.size(), VK_NULL_HANDLE);
        for (std::size_t i = 0; i < images.size(); ++i) {
            VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            result = Check(vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &renderFinished[i]), "Create present semaphore");
            if (!result) return result;
            auto record = std::make_unique<ImageRecord>();
            record->device = device.device; record->image = images[i]; record->view = std::exchange(views[i], VK_NULL_HANDLE); record->external = true;
            record->desc = {"Swapchain color", swapchain.extent.width, swapchain.extent.height, Format::Swapchain, ImageUsage::ColorAttachment};
            swapchainHandles.push_back(imageResources.Insert(std::move(record)));
            auto suffix = " " + std::to_string(i);
            Name(VK_OBJECT_TYPE_IMAGE, images[i], ("Swapchain image" + suffix).c_str());
            Name(VK_OBJECT_TYPE_IMAGE_VIEW, imageResources.Get(swapchainHandles.back())->view, ("Swapchain image view" + suffix).c_str());
            Name(VK_OBJECT_TYPE_SEMAPHORE, renderFinished[i], ("Render finished" + suffix).c_str());
        }
        if (previousFormat != swapchain.image_format) {
            pipelines.ForEach([&](PipelineRecord& record) {
                if (!result || record.compute||record.desc.colorFormat != Format::Swapchain) return;
                if (record.pipeline) vkDestroyPipeline(device.device, record.pipeline, nullptr);
                record.pipeline = VK_NULL_HANDLE; result = BuildPipeline(record);
            });
            if (!result) return result;
        }
        requestedWidth = extent.width;
        requestedHeight = extent.height;
        recreate = false;
        ++generations;
        Log::Info("Swapchain generation " + std::to_string(generations) + ": "
            + std::to_string(swapchain.extent.width) + "x" + std::to_string(swapchain.extent.height));
        return Result::Success();
    }

    Result BeginFrame(FrameInfo& output, bool& available) {
        available = false;
        if (active) return Result::Failure("Frame already active");
        if (!ready) return Result::Failure("Device is not initialized");
        const auto extent = window->FramebufferExtent();
        if (extent.width <= 0 || extent.height <= 0) return Result::Success();
        if (recreate || extent.width != requestedWidth || extent.height != requestedHeight || !swapchain.swapchain) {
            auto result = RecreateSwapchain();
            if (!result) return result;
        }
        auto& frame = frames[frameIndex];
        auto result = Check(vkWaitForFences(device.device, 1, &frame.fence, VK_TRUE, UINT64_MAX), "Wait for frame fence");
        if (!result) return result;
        if (frame.submitted) {
            std::vector<std::uint64_t> timestamps(2 + 2 * frame.passNames.size());
            result = Check(vkGetQueryPoolResults(device.device, frame.timestamps, 0, static_cast<std::uint32_t>(timestamps.size()), timestamps.size() * sizeof(std::uint64_t),
                timestamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "Read GPU timestamps");
            if (!result) return result;
            const auto mask = timestampBits == 64 ? UINT64_MAX : ((std::uint64_t{1} << timestampBits) - 1);
            gpuFrameMilliseconds = static_cast<double>((timestamps[1] - timestamps[0]) & mask) * timestampPeriod / 1.0e6;
            passTimings.clear();
            for (std::size_t i = 0; i < frame.passNames.size(); ++i)
                passTimings.push_back({frame.passNames[i], static_cast<double>((timestamps[3 + i*2] - timestamps[2 + i*2]) & mask) * timestampPeriod / 1.0e6});
        }
        std::uint32_t imageIndex = 0;
        auto acquired = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX,
            frame.acquired, VK_NULL_HANDLE, &imageIndex);
        if (acquired == VK_ERROR_OUT_OF_DATE_KHR) { recreate = true; return Result::Success(); }
        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) return Check(acquired, "Acquire image");
        if (acquired == VK_SUBOPTIMAL_KHR) recreate = true;
        result = Check(vkResetCommandPool(device.device, frame.pool, 0), "Reset command pool");
        if (!result) return result;
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = Check(vkBeginCommandBuffer(frame.commands, &begin), "Begin command buffer");
        if (!result) return result;
        vkCmdResetQueryPool(frame.commands, frame.timestamps, 0, 2 + 2 * kMaxTimedPasses);
        vkCmdWriteTimestamp2(frame.commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.timestamps, 0);
        frame.passNames.clear(); currentImage = imageIndex; active = true; renderingActive = false;++frameSerial;
        boundPipeline = {}; meshBound = false; tableBound = false; constantsBound = false;
        output = {swapchainHandles[imageIndex], swapchain.extent.width, swapchain.extent.height, static_cast<std::uint32_t>(frameIndex)};
        available = true; return Result::Success();
    }
    Result EndFrame() {
        if (!active || renderingActive || passActive || imageResources.Get(swapchainHandles[currentImage])->state != ResourceState::Present)
            return Result::Failure("Frame must end outside passes with color in Present state");
        auto& frame = frames[frameIndex]; const auto imageIndex = currentImage;
        auto result = Result::Success();
        vkCmdWriteTimestamp2(frame.commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.timestamps, 1);
        result = Check(vkEndCommandBuffer(frame.commands), "End command buffer");
        if (!result) return result;
        VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        wait.semaphore = frame.acquired;
        wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        signal.semaphore = renderFinished[imageIndex];
        signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        VkCommandBufferSubmitInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        command.commandBuffer = frame.commands;
        VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit.waitSemaphoreInfoCount = 1;
        submit.pWaitSemaphoreInfos = &wait;
        submit.commandBufferInfoCount = 1;
        submit.pCommandBufferInfos = &command;
        submit.signalSemaphoreInfoCount = 1;
        submit.pSignalSemaphoreInfos = &signal;
        result = Check(vkResetFences(device.device, 1, &frame.fence), "Reset frame fence");
        if (!result) return result;
        result = Check(vkQueueSubmit2(graphicsQueue, 1, &submit, frame.fence), "Submit frame");
        if (!result) return result;
        frame.submitted = true;
        active = false;
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished[imageIndex];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain.swapchain;
        present.pImageIndices = &imageIndex;
        const auto presented = vkQueuePresentKHR(presentQueue, &present);
        frameIndex = (frameIndex + 1) % kFramesInFlight;
        if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) recreate = true;
        else if (presented != VK_SUCCESS) return Check(presented, "Present frame");
        if (presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR) ++presentedFrames;
        return Result::Success();
    }

    Result CreatePipeline(const PipelineDesc& desc, PipelineHandle& output) {
        VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(device.physical_device, &props);
        if (!ready || desc.name.empty() || desc.pushConstantBytes % 4 || desc.pushConstantBytes > props.limits.maxPushConstantsSize ||
            desc.colorFormat == Format::D32Float || desc.vertexStride > props.limits.maxVertexInputBindingStride || desc.attributes.size() > props.limits.maxVertexInputAttributes)
            return Result::Failure("Invalid pipeline descriptor");
        for (std::size_t i = 0; i < desc.attributes.size(); ++i) {
            const auto& attribute = desc.attributes[i];
            const auto bytes = attribute.format == VertexFormat::Float2 ? 8u : attribute.format == VertexFormat::Float3 ? 12u : 16u;
            if (attribute.location >= props.limits.maxVertexInputAttributes || attribute.offset > props.limits.maxVertexInputAttributeOffset ||
                static_cast<std::uint64_t>(attribute.offset) + bytes > desc.vertexStride) return Result::Failure("Invalid vertex attribute");
            for (std::size_t previous = 0; previous < i; ++previous)
                if (desc.attributes[previous].location == attribute.location) return Result::Failure("Duplicate vertex attribute location");
        }
        auto record = std::make_unique<PipelineRecord>(); record->device = device.device; record->desc = desc;
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, desc.pushConstantBytes};
        VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        info.setLayoutCount = 1; info.pSetLayouts = &textureLayout;
        info.pushConstantRangeCount = desc.pushConstantBytes ? 1u : 0u; info.pPushConstantRanges = &range;
        auto result = Check(vkCreatePipelineLayout(device.device, &info, nullptr, &record->layout), "Create pipeline layout");
        if (!result) return result;
        Name(VK_OBJECT_TYPE_PIPELINE_LAYOUT, record->layout, (desc.name + " layout").c_str());
        result = BuildPipeline(*record); if (!result) return result;
        output = pipelines.Insert(std::move(record)); return Result::Success();
    }
    Result CreateComputePipeline(const ComputePipelineDesc& desc,PipelineHandle& output){output={};VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(device.physical_device,&props);if(!ready||desc.name.empty()||desc.shader.empty()||desc.pushConstantBytes%4||desc.pushConstantBytes>props.limits.maxPushConstantsSize)return Result::Failure("Invalid compute pipeline");auto record=std::make_unique<PipelineRecord>();record->device=device.device;record->compute=true;record->desc.name=desc.name;record->desc.pushConstantBytes=desc.pushConstantBytes;VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,desc.pushConstantBytes};VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.setLayoutCount=1;layout.pSetLayouts=&textureLayout;layout.pushConstantRangeCount=desc.pushConstantBytes?1u:0u;layout.pPushConstantRanges=&range;auto result=Check(vkCreatePipelineLayout(device.device,&layout,nullptr,&record->layout),"Create compute layout");if(!result)return result;VkShaderModule shader{};result=LoadShader(desc.shader,desc.name.c_str(),shader);if(!result)return result;VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};info.layout=record->layout;info.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;info.stage.module=shader;info.stage.pName="main";result=Check(vkCreateComputePipelines(device.device,VK_NULL_HANDLE,1,&info,nullptr,&record->pipeline),"Create compute pipeline");vkDestroyShaderModule(device.device,shader,nullptr);if(!result)return result;Name(VK_OBJECT_TYPE_PIPELINE,record->pipeline,desc.name.c_str());output=pipelines.Insert(std::move(record));return Result::Success();}
    Result BeginPass(std::string_view name) override {
        return Guard([&] {
            auto& frame = frames[frameIndex];
            if (!active || passActive || name.empty() || frame.passNames.size() >= kMaxTimedPasses)
                return Result::Failure("Invalid pass begin");
            frame.passNames.emplace_back(name); passActive = true;
            VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT}; label.pLabelName = frame.passNames.back().c_str();
            label.color[0] = 0.2f; label.color[1] = 0.5f; label.color[2] = 1; label.color[3] = 1;
            if (vkCmdBeginDebugUtilsLabelEXT) vkCmdBeginDebugUtilsLabelEXT(frame.commands, &label);
            vkCmdWriteTimestamp2(frame.commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.timestamps,
                2 + 2 * static_cast<std::uint32_t>(frame.passNames.size() - 1));
            return Result::Success();
        });
    }
    Result EndPass() override {
        if (!passActive || renderingActive) return Result::Failure("Invalid pass end");
        auto& frame = frames[frameIndex];
        vkCmdWriteTimestamp2(frame.commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frame.timestamps,
            3 + 2 * static_cast<std::uint32_t>(frame.passNames.size() - 1));
        if (vkCmdEndDebugUtilsLabelEXT) vkCmdEndDebugUtilsLabelEXT(frame.commands);
        passActive = false; return Result::Success();
    }
    Result Barriers(std::span<const ImageBarrier> imageBarriers, std::span<const BufferBarrier> bufferBarriers) override {
        return Guard([&] {
            if (!active || renderingActive) return Result::Failure("Barriers require an active frame outside rendering");
            std::vector<VkImageMemoryBarrier2> nativeImages;
            std::vector<VkBufferMemoryBarrier2> nativeBuffers;
            for (const auto& transition : imageBarriers) {
                auto* record = imageResources.Get(transition.image);
                if (!record || transition.after == ResourceState::Undefined || transition.after == ResourceState::VertexRead || transition.after == ResourceState::IndexRead || transition.after == ResourceState::StorageRead || transition.after == ResourceState::HostWrite||(transition.after==ResourceState::StorageReadWrite&&!HasFlag(record->desc.usage,ImageUsage::Storage)))
                    return Result::Failure("Invalid image transition");
                VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                barrier.srcStageMask = record->external && (record->state == ResourceState::Undefined || record->state == ResourceState::Present)
                    ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : Stages(record->state);
                barrier.srcAccessMask = Access(record->state);
                barrier.dstStageMask = Stages(transition.after); barrier.dstAccessMask = Access(transition.after);
                barrier.oldLayout = transition.discard ? VK_IMAGE_LAYOUT_UNDEFINED : Layout(record->state);
                barrier.newLayout = Layout(transition.after); barrier.image = record->image;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange = {static_cast<VkImageAspectFlags>(record->desc.format == Format::D32Float ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 1, 0, record->desc.dimension==ImageDimension::Cube?6u:1u};
                nativeImages.push_back(barrier);
            }
            for (const auto& transition : bufferBarriers) {
                auto* record = buffers.Get(transition.buffer);
                if (!record || transition.after == ResourceState::Undefined || transition.after == ResourceState::HostWrite || transition.after == ResourceState::Present || transition.after == ResourceState::ColorWrite || transition.after == ResourceState::DepthWrite || transition.after == ResourceState::SampledRead ||
                    ((transition.after == ResourceState::StorageRead||transition.after==ResourceState::StorageReadWrite) && !HasFlag(record->desc.usage,BufferUsage::Storage)))
                    return Result::Failure("Invalid buffer transition");
                VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
                barrier.srcStageMask = Stages(record->state); barrier.srcAccessMask = Access(record->state);
                barrier.dstStageMask = Stages(transition.after); barrier.dstAccessMask = Access(transition.after);
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = record->buffer; barrier.size = VK_WHOLE_SIZE; nativeBuffers.push_back(barrier);
            }
            VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            info.imageMemoryBarrierCount = static_cast<std::uint32_t>(nativeImages.size()); info.pImageMemoryBarriers = nativeImages.data();
            info.bufferMemoryBarrierCount = static_cast<std::uint32_t>(nativeBuffers.size()); info.pBufferMemoryBarriers = nativeBuffers.data();
            vkCmdPipelineBarrier2(frames[frameIndex].commands, &info);
            for (const auto& transition : imageBarriers) imageResources.Get(transition.image)->state = transition.after;
            for (const auto& transition : bufferBarriers) buffers.Get(transition.buffer)->state = transition.after;
            return Result::Success();
        });
    }
    Result BeginRendering(const RenderingDesc& desc) override {
        auto* color=imageResources.Get(desc.color);auto* depth=imageResources.Get(desc.depth);auto* target=color?color:depth;
        if(!passActive||renderingActive||!target||(color&&color->state!=ResourceState::ColorWrite)||(depth&&depth->state!=ResourceState::DepthWrite)||(color&&depth&&(color->desc.width!=depth->desc.width||color->desc.height!=depth->desc.height)))return Result::Failure("Invalid rendering attachments");
        std::uint32_t width=desc.width?desc.width:target->desc.width,height=desc.height?desc.height:target->desc.height;
        if(desc.x+width>target->desc.width||desc.y+height>target->desc.height)return Result::Failure("Rendering region exceeds attachment");
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        if(color){attachment.imageView=color->view;attachment.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;attachment.loadOp=desc.loadColor?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;std::copy(desc.clearColor.begin(),desc.clearColor.end(),attachment.clearValue.color.float32);}
        VkRenderingAttachmentInfo depthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        if(depth){depthAttachment.imageView=depth->view;depthAttachment.imageLayout=VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;depthAttachment.loadOp=desc.loadDepth?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR;depthAttachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;depthAttachment.clearValue.depthStencil.depth=desc.clearDepth;}
        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};info.renderArea={{static_cast<int>(desc.x),static_cast<int>(desc.y)},{width,height}};info.layerCount=1;info.colorAttachmentCount=color?1u:0u;info.pColorAttachments=color?&attachment:nullptr;info.pDepthAttachment=depth?&depthAttachment:nullptr;
        auto commands=frames[frameIndex].commands;vkCmdBeginRendering(commands,&info);VkViewport viewport{float(desc.x),float(desc.y),float(width),float(height),0,1};VkRect2D scissor=info.renderArea;vkCmdSetViewport(commands,0,1,&viewport);vkCmdSetScissor(commands,0,1,&scissor);renderingActive=true;return Result::Success();
    }
    Result EndRendering() override {
        if (!renderingActive) return Result::Failure("Rendering is not active");
        vkCmdEndRendering(frames[frameIndex].commands); renderingActive = false; return Result::Success();
    }
    Result BindPipeline(PipelineHandle handle) override {
        auto* record = pipelines.Get(handle);
        if (!active||!passActive||!record||(record->compute?renderingActive:!renderingActive)) return Result::Failure("Invalid pipeline binding");
        vkCmdBindPipeline(frames[frameIndex].commands, record->compute?VK_PIPELINE_BIND_POINT_COMPUTE:VK_PIPELINE_BIND_POINT_GRAPHICS, record->pipeline);
        boundPipeline = handle; constantsBound = record->desc.pushConstantBytes == 0; tableBound = false; return Result::Success();
    }
    Result BindTextureTable(TextureTableHandle handle) override {
        auto* table = tables.Get(handle); auto* pipeline = pipelines.Get(boundPipeline);
        if (!active||!passActive||!table||!pipeline||(pipeline->compute?renderingActive:!renderingActive)) return Result::Failure("Invalid texture table binding");
        for (std::size_t i = 0; i < table->images.size(); ++i) {
            if (!table->images[i]) continue;
            auto* image = imageResources.Get(table->images[i]);
            if (!image || !samplers.Get(table->samplers[i<256?i:(i-256)%8]) || image->state != ResourceState::SampledRead)
                return Result::Failure("Texture table contains a stale or unreadable resource");
        }
        for(auto storageHandle:table->storage)if(storageHandle){auto image=imageResources.Get(storageHandle);if(!image||image->state!=ResourceState::StorageReadWrite)return Result::Failure("Storage table contains a stale or unwritable image");}
        vkCmdBindDescriptorSets(frames[frameIndex].commands, pipeline->compute?VK_PIPELINE_BIND_POINT_COMPUTE:VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 1, &table->set, 0, nullptr);
        table->lastSerial=frameSerial;table->lastSlot=frameIndex;
        tableBound = true; return Result::Success();
    }
    Result BindMesh(BufferHandle vertexHandle, BufferHandle indexHandle) override {
        auto* vertices = buffers.Get(vertexHandle); auto* indices = buffers.Get(indexHandle);
        if (!renderingActive || !vertices || !indices || vertices->state != ResourceState::VertexRead || indices->state != ResourceState::IndexRead)
            return Result::Failure("Invalid mesh buffers");
        VkDeviceSize offset = 0; auto commands = frames[frameIndex].commands;
        vkCmdBindVertexBuffers(commands, 0, 1, &vertices->buffer, &offset);
        vkCmdBindIndexBuffer(commands, indices->buffer, 0, VK_INDEX_TYPE_UINT32);
        boundIndexBytes = indices->desc.bytes; meshBound = true; return Result::Success();
    }
    std::uint64_t boundIndexBytes = 0;
    Result PushConstants(std::span<const std::byte> bytes) override {
        auto* pipeline = pipelines.Get(boundPipeline);
        if (!active||!passActive||!pipeline||(pipeline->compute?renderingActive:!renderingActive)||bytes.size() != pipeline->desc.pushConstantBytes)
            return Result::Failure("Invalid push constants");
        if (bytes.empty()) return Result::Success();
        vkCmdPushConstants(frames[frameIndex].commands, pipeline->layout, pipeline->compute?VK_SHADER_STAGE_COMPUTE_BIT:VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, static_cast<std::uint32_t>(bytes.size()), bytes.data());
        constantsBound = true; return Result::Success();
    }
    Result DrawIndexed(std::uint32_t count, std::uint32_t first, std::uint32_t instances, std::uint32_t firstInstance) override {
        if (!renderingActive || !boundPipeline || !meshBound || !tableBound || !constantsBound ||
            (static_cast<std::uint64_t>(first) + count) * 4 > boundIndexBytes)
            return Result::Failure("Invalid indexed draw");
        vkCmdDrawIndexed(frames[frameIndex].commands, count, instances, first, 0, firstInstance); return Result::Success();
    }
    Result Dispatch(std::uint32_t x,std::uint32_t y,std::uint32_t z) override{auto pipeline=pipelines.Get(boundPipeline);VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(device.physical_device,&props);if(!active||!passActive||renderingActive||!pipeline||!pipeline->compute||!tableBound||!constantsBound||!x||!y||!z||x>props.limits.maxComputeWorkGroupCount[0]||y>props.limits.maxComputeWorkGroupCount[1]||z>props.limits.maxComputeWorkGroupCount[2])return Result::Failure("Invalid compute dispatch");vkCmdDispatch(frames[frameIndex].commands,x,y,z);return Result::Success();}
    template<class Pool, class HandleType> Result DestroyResource(Pool& pool, HandleType handle) {
        if (!pool.Get(handle)) return Result::Failure("Stale resource handle");
        if (active) return Result::Failure("Cannot destroy a resource while recording a frame");
        auto result = Check(vkDeviceWaitIdle(device.device), "Wait before resource destruction");
        if (result) pool.Remove(handle);
        return result;
    }
    Result GuiInitialize() {
        if (!ready || guiReady || !ImGui::GetCurrentContext()) return Result::Failure("Invalid UI initialization");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,512};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;pool.maxSets=512;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        auto result=Check(vkCreateDescriptorPool(device.device,&pool,nullptr,&guiPool),"Create UI descriptor pool");if (!result) return result;
        Name(VK_OBJECT_TYPE_DESCRIPTOR_POOL,guiPool,"Editor image descriptors");
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        result=Check(vkCreateSampler(device.device,&sampler,nullptr,&guiSampler),"Create UI sampler");if (!result) return result;
        Name(VK_OBJECT_TYPE_SAMPLER,guiSampler,"Editor viewport sampler");
        ImGui_ImplVulkan_InitInfo info{};info.ApiVersion=VK_API_VERSION_1_3;info.Instance=instance.instance;info.PhysicalDevice=device.physical_device;
        info.Device=device.device;info.QueueFamily=graphicsFamily;info.Queue=graphicsQueue;info.DescriptorPool=guiPool;
        info.MinImageCount=kFramesInFlight;info.ImageCount=kFramesInFlight;info.MSAASamples=VK_SAMPLE_COUNT_1_BIT;info.UseDynamicRendering=true;
        guiFormat=swapchain.image_format;
        info.PipelineRenderingCreateInfo={VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};info.PipelineRenderingCreateInfo.colorAttachmentCount=1;info.PipelineRenderingCreateInfo.pColorAttachmentFormats=&guiFormat;
        info.CheckVkResultFn=[](VkResult result){ if (result!=VK_SUCCESS) Log::Error("ImGui Vulkan operation failed: "+std::to_string(result)); };
        if (!ImGui_ImplVulkan_Init(&info)) return Result::Failure("ImGui Vulkan initialization failed");
        guiReady=true;return Result::Success();
    }
    void GuiShutdown() noexcept {
        if (device.device) vkDeviceWaitIdle(device.device);
        if (guiReady) ImGui_ImplVulkan_Shutdown();guiReady=false;
        if (guiSampler) vkDestroySampler(device.device,guiSampler,nullptr);guiSampler=VK_NULL_HANDLE;
        if (guiPool) vkDestroyDescriptorPool(device.device,guiPool,nullptr);guiPool=VK_NULL_HANDLE;
    }

    Result Shutdown() noexcept {
        ready = false;
        VkResult idleResult = VK_SUCCESS;
        if (device.device) {
            idleResult = vkDeviceWaitIdle(device.device);
            GuiShutdown();
            DestroySwapchain();
            for (auto& frame : frames) {
                if (frame.timestamps) vkDestroyQueryPool(device.device, frame.timestamps, nullptr);
                if (frame.fence) vkDestroyFence(device.device, frame.fence, nullptr);
                if (frame.acquired) vkDestroySemaphore(device.device, frame.acquired, nullptr);
                if (frame.pool) vkDestroyCommandPool(device.device, frame.pool, nullptr);
                frame = {};
            }
            tables.Clear(); pipelines.Clear(); samplers.Clear(); imageResources.Clear(); buffers.Clear();
            if (textureLayout) vkDestroyDescriptorSetLayout(device.device, textureLayout, nullptr);
            textureLayout = VK_NULL_HANDLE;
            if (allocator) vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
            vkb::destroy_device(device);
            device = {};
        }
        if (surface) vkDestroySurfaceKHR(instance.instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
        if (instance.instance) vkb::destroy_instance(instance);
        instance = {};
        if (idleResult != VK_SUCCESS) return Result::Failure("Vulkan shutdown wait failed");
        if (validationMessages.load() != 0) return Result::Failure("Vulkan validation reported warning/error messages");
        return Result::Success();
    }
};
Device::Device() : m_impl(std::make_unique<Impl>()) {}
Device::~Device() { auto result = m_impl->Shutdown(); if (!result) Log::Error(result.Error()); }
Result Device::Initialize(Window& window,std::string_view gpuOverride) { return Guard([&] { return m_impl->Initialize(window,gpuOverride); }); }
const AdapterInfo& Device::Adapter() const noexcept {return m_impl->adapter;}
#define ENGINE_CREATE(Name, Desc, Handle) Result Device::Name(const Desc& desc, Handle& output) { return Guard([&] { return m_impl->Name(desc, output); }); }
ENGINE_CREATE(CreateBuffer, BufferDesc, BufferHandle)
ENGINE_CREATE(CreateImage, ImageDesc, ImageHandle)
ENGINE_CREATE(CreateSampler, SamplerDesc, SamplerHandle)
ENGINE_CREATE(CreatePipeline, PipelineDesc, PipelineHandle)
#undef ENGINE_CREATE
Result Device::CreateTextureTable(std::string_view name, TextureTableHandle& output) { return Guard([&] { return m_impl->CreateTextureTable(name, output); }); }
Result Device::CreateComputePipeline(const ComputePipelineDesc& desc,PipelineHandle& output){return Guard([&]{return m_impl->CreateComputePipeline(desc,output);});}
Result Device::WriteStorageImage(TextureTableHandle table,std::uint32_t index,ImageHandle image){return Guard([&]{return m_impl->WriteStorageImage(table,index,image);});}
Result Device::SetTexture(TextureTableHandle table, std::uint32_t index, ImageHandle image, SamplerHandle sampler) { return Guard([&] { return m_impl->SetTexture(table,index,image,sampler); }); }
Result Device::WriteTexture(TextureTableHandle table,std::uint32_t index,ImageHandle image,SamplerHandle sampler) { return Guard([&]{return m_impl->SetTexture(table,index,image,sampler,true);}); }
Result Device::UploadBuffer(BufferHandle buffer, std::span<const std::byte> bytes, ResourceState state) { return Guard([&] { return m_impl->UploadBuffer(buffer,bytes,state); }); }
Result Device::UploadImage(ImageHandle image, std::span<const std::byte> bytes) { return Guard([&] { return m_impl->UploadImage(image,bytes); }); }
Result Device::ReadImage(ImageHandle image,std::vector<std::byte>& pixels){return Guard([&]{return m_impl->ReadImage(image,pixels);});}
Result Device::WriteBuffer(BufferHandle handle, std::span<const std::byte> bytes,std::uint64_t offset) { return Guard([&] {
    auto* record=m_impl->buffers.Get(handle);
    if (!m_impl->active || !record || !record->desc.hostVisible || bytes.empty() || offset>record->desc.bytes || bytes.size()>record->desc.bytes-offset)
        return Result::Failure("Invalid frame-slot buffer write");
    auto result=m_impl->FillStaging(handle,bytes,offset); if (result) record->state=ResourceState::HostWrite; return result;
}); }
Result Device::ReadBuffer(BufferHandle handle,std::span<std::byte> bytes,std::uint64_t offset){return Guard([&]{
    auto* record=m_impl->buffers.Get(handle);if(!m_impl->active||!record||!record->desc.hostVisible||bytes.empty()||offset>record->desc.bytes||bytes.size()>record->desc.bytes-offset)return Result::Failure("Invalid fenced buffer read");
    void* mapped=nullptr;auto result=Check(vmaMapMemory(m_impl->allocator,record->allocation,&mapped),"Map readback");if(!result)return result;
    result=Check(vmaInvalidateAllocation(m_impl->allocator,record->allocation,offset,bytes.size()),"Invalidate readback");if(result)std::memcpy(bytes.data(),static_cast<std::byte*>(mapped)+offset,bytes.size());vmaUnmapMemory(m_impl->allocator,record->allocation);return result;
});}
#define ENGINE_DESTROY(Handle, Pool) Result Device::Destroy(Handle handle) { return Guard([&] { return m_impl->DestroyResource(m_impl->Pool, handle); }); }
ENGINE_DESTROY(BufferHandle, buffers)
ENGINE_DESTROY(SamplerHandle, samplers)
ENGINE_DESTROY(PipelineHandle, pipelines)
ENGINE_DESTROY(TextureTableHandle, tables)
#undef ENGINE_DESTROY
Result Device::Destroy(ImageHandle handle) { return Guard([&] {
    auto* record = m_impl->imageResources.Get(handle);
    if (record && record->external) return Result::Failure("Swapchain images are borrowed");
    return m_impl->DestroyResource(m_impl->imageResources,handle);
}); }
ResourceState Device::StateOf(BufferHandle handle) const noexcept { auto* record = m_impl->buffers.Get(handle); return record ? record->state : ResourceState::Undefined; }
ResourceState Device::StateOf(ImageHandle handle) const noexcept { auto* record = m_impl->imageResources.Get(handle); return record ? record->state : ResourceState::Undefined; }
std::uint64_t Device::BufferAddress(BufferHandle handle) const noexcept {
    auto* record = m_impl->buffers.Get(handle); if (!record || !HasFlag(record->desc.usage, BufferUsage::DeviceAddress)) return 0;
    VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; info.buffer = record->buffer;
    return vkGetBufferDeviceAddress(m_impl->device.device,&info);
}
Result Device::BeginFrame(FrameInfo& frame, bool& available) { return Guard([&] { return m_impl->BeginFrame(frame, available); }); }
CommandList& Device::Commands() noexcept { return *m_impl; }
Result Device::EndFrame() { return Guard([&] { return m_impl->EndFrame(); }); }
Result Device::WaitIdle() { return m_impl->device.device ? Check(vkDeviceWaitIdle(m_impl->device.device), "Wait idle") : Result::Success(); }
std::span<const PassTiming> Device::PassTimings() const noexcept { return m_impl->passTimings; }

Result Device::GuiInitialize() { return Guard([&]{return m_impl->GuiInitialize();}); }
bool Device::GuiUsesSrgb() const noexcept {return m_impl->guiFormat==VK_FORMAT_B8G8R8A8_SRGB||m_impl->guiFormat==VK_FORMAT_R8G8B8A8_SRGB;}
Result Device::GuiRebuildFonts() {return Guard([&]{
    if(m_impl->active||!m_impl->guiReady) return Result::Failure("Rebuild fonts outside an active UI frame");
    auto result=WaitIdle();if(result) ImGui_ImplVulkan_DestroyFontsTexture();return result;
});}
Result Device::GuiNewFrame() { return Guard([&]{
    if (!m_impl->guiReady) return Result::Failure("UI backend is not initialized");
    ImGui_ImplVulkan_NewFrame();return Result::Success();
}); }
Result Device::GuiTexture(ImageHandle handle,std::uint64_t& output) { return Guard([&]{
    auto* image=m_impl->imageResources.Get(handle);if (!m_impl->guiReady || !image || !HasFlag(image->desc.usage,ImageUsage::Sampled)) return Result::Failure("Invalid UI image");
    auto descriptor=ImGui_ImplVulkan_AddTexture(m_impl->guiSampler,image->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (!descriptor) return Result::Failure("UI image descriptor allocation failed");
    m_impl->Name(VK_OBJECT_TYPE_DESCRIPTOR_SET,descriptor,"Viewport UI texture");output=reinterpret_cast<std::uint64_t>(descriptor);return Result::Success();
}); }
Result Device::GuiRemoveTexture(std::uint64_t texture) {
    if (m_impl->active) return Result::Failure("Retire UI textures outside frame recording");
    auto result=WaitIdle();if (result && texture) ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(texture));return result;
}
Result Device::GuiDraw(void* drawData,ImageHandle handle) { return Guard([&]{
    auto* image=m_impl->imageResources.Get(handle);
    if (!m_impl->guiReady || !drawData || !image || m_impl->guiFormat!=m_impl->swapchain.image_format) return Result::Failure("UI output format changed or backend unavailable");
    auto result=m_impl->BeginRendering({handle,{}});if (!result) return result;
    ImGui_ImplVulkan_RenderDrawData(static_cast<ImDrawData*>(drawData),m_impl->frames[m_impl->frameIndex].commands);
    return m_impl->EndRendering();
}); }
void Device::GuiShutdown() noexcept { m_impl->GuiShutdown(); }
Result Device::Shutdown() noexcept { return m_impl->Shutdown(); }
DeviceStatistics Device::Statistics() const noexcept { DeviceStatistics result{m_impl->presentedFrames,m_impl->generations,m_impl->validationMessages.load(),m_impl->gpuFrameMilliseconds};result.driverMemoryBudget=m_impl->driverMemoryBudget;if(m_impl->allocator){std::array<VmaBudget,VK_MAX_MEMORY_HEAPS> budgets{};vmaGetHeapBudgets(m_impl->allocator,budgets.data());VkPhysicalDeviceMemoryProperties properties{};vkGetPhysicalDeviceMemoryProperties(m_impl->device.physical_device,&properties);for(std::uint32_t i=0;i<properties.memoryHeapCount;++i)if(properties.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT){result.memoryUsedBytes+=budgets[i].usage;result.memoryBudgetBytes+=budgets[i].budget;}}return result; }
}
