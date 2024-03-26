// The implementation of the graphics layer using Vulkan 1.3.

/* IMPORTANT INFORMATION
 *
 * When allocating memory with  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
 * always set a memory priority. This feature uses the device extension
 * VK_EXT_memory_priority. Depth buffers have a priority of 1.0f. Other,
 * non-essential allocations will have a priority of 0.5f.
 *
 * Call vkResetCommandPool before reusing it in another frame.
 * Otherwise, the pool will keep on growing until you run out of memory.
 * - NVIDIA Vulkan Dos and Don'ts
 *
 */

/* TODO
 *
 * - Support index buffers of both UINT16 and UINT32 types
 * - Allow textures, samplers, etc to be created after rendering has begun
 * - Use pipeline cache
 *
 */

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>
#include <deque>
#include <map>
#include <iostream>

#include <SDL_vulkan.h>

#include <shaderc/shaderc.hpp>

#include <volk.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"

#include "gfx_device.h"
#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/gpu_allocator.h"
#include "vulkan/swapchain.h"
#include "util.h"
#include "config.h"
#include "log.h"
#include "util/files.h"

static constexpr bool flip_viewport = false;

inline static void checkVulkanError(VkResult errorCode, int lineNo)
{
    if (errorCode != VK_SUCCESS) {
        const std::string message("VULKAN ERROR ON LINE " + std::to_string(lineNo));
        throw std::runtime_error(message.c_str());
    }
}

static void check_vk_result(VkResult code) { checkVulkanError(code, -1); }

#undef VKCHECK
#define VKCHECK(ErrCode) checkVulkanError(ErrCode, __LINE__)

namespace engine {

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;       // This improved FPS by 5x! (on Intel IGPU)

static constexpr size_t PUSH_CONSTANT_MAX_SIZE = 128; // bytes
static constexpr VkIndexType INDEX_TYPE = VK_INDEX_TYPE_UINT32;

static constexpr int kShadowmapSize = 2048;

// structures and enums

struct FrameData {
    VkFence renderFence = VK_NULL_HANDLE;
    VkSemaphore transferSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    VkSemaphore presentSemaphore = VK_NULL_HANDLE;

    VkCommandPool graphicsPool = VK_NULL_HANDLE;
    VkCommandBuffer drawBuf = VK_NULL_HANDLE;

    VkCommandPool transferPool = VK_NULL_HANDLE;
    VkCommandBuffer transferBuf = VK_NULL_HANDLE;
};

// handles

struct gfx::Buffer {
    gfx::BufferType type;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VkDeviceSize size = 0;
};

struct gfx::Pipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline handle = VK_NULL_HANDLE;
};

struct gfx::Image {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
};

struct gfx::Sampler {
    VkSampler sampler = VK_NULL_HANDLE;
};

struct gfx::DrawBuffer {
    FrameData frameData{};
    uint32_t currentFrameIndex = 0; // corresponds to the frameData
    uint32_t imageIndex = 0;        // for swapchain present
};

struct gfx::DescriptorSetLayout {
    VkDescriptorSetLayout layout;
};

struct gfx::DescriptorSet {
    std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> sets; // frames in flight cannot use the same descriptor set in case the
                                                        // buffer needs updating
};

struct gfx::UniformBuffer {
    gfx::Buffer stagingBuffer{};
    std::array<gfx::Buffer, FRAMES_IN_FLIGHT> gpuBuffers;
};

// enum converters

namespace converters {

static VkFormat getVertexAttribFormat(gfx::VertexAttribFormat fmt)
{
    switch (fmt) {
        case gfx::VertexAttribFormat::kFloat2:
            return VK_FORMAT_R32G32_SFLOAT;
        case gfx::VertexAttribFormat::kFloat3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case gfx::VertexAttribFormat::kFloat4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    throw std::runtime_error("Unknown vertex attribute format");
}

static VkBufferUsageFlagBits getBufferUsageFlag(gfx::BufferType type)
{
    switch (type) {
        case gfx::BufferType::kVertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case gfx::BufferType::kIndex:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case gfx::BufferType::kUniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        default:
            throw std::runtime_error("This buffer type does not have usage bits");
    }
}

[[maybe_unused]] static VkFilter getFilter(gfx::Filter filter)
{
    switch (filter) {
        case gfx::Filter::kLinear:
            return VK_FILTER_LINEAR;
        case gfx::Filter::kNearest:
            return VK_FILTER_NEAREST;
    }
    throw std::runtime_error("Unknown filter");
}

[[maybe_unused]] static VkSamplerAddressMode getSamplerAddressMode(gfx::WrapMode mode)
{
    switch (mode) {
        case gfx::WrapMode::kRepeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case gfx::WrapMode::kMirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case gfx::WrapMode::kClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    throw std::runtime_error("Unknown wrap mode");
}

[[maybe_unused]] static VkSamplerMipmapMode getSamplerMipmapMode(gfx::Filter filter)
{
    switch (filter) {
        case gfx::Filter::kLinear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case gfx::Filter::kNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
    throw std::runtime_error("Unknown filter");
}

[[maybe_unused]] static VkSampleCountFlags getSampleCountFlags(gfx::MSAALevel level)
{
    switch (level) {
        case gfx::MSAALevel::kOff:
            return VK_SAMPLE_COUNT_1_BIT;
        case gfx::MSAALevel::k2X:
            return VK_SAMPLE_COUNT_2_BIT;
        case gfx::MSAALevel::k4X:
            return VK_SAMPLE_COUNT_4_BIT;
        case gfx::MSAALevel::k8X:
            return VK_SAMPLE_COUNT_8_BIT;
        case gfx::MSAALevel::k16X:
            return VK_SAMPLE_COUNT_16_BIT;
        default:
            throw std::runtime_error("Unknown MSAA level");
    }
}

static VkDescriptorType getDescriptorType(gfx::DescriptorType type)
{
    switch (type) {
        case gfx::DescriptorType::kUniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case gfx::DescriptorType::kCombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        default:
            throw std::runtime_error("Unknown descriptor type");
    }
}

static VkShaderStageFlags getShaderStageFlags(gfx::ShaderStageFlags::Flags flags)
{
    VkShaderStageFlags out = 0;
    if (flags & gfx::ShaderStageFlags::kVertex) out |= VK_SHADER_STAGE_VERTEX_BIT;
    if (flags & gfx::ShaderStageFlags::kFragment) out |= VK_SHADER_STAGE_FRAGMENT_BIT;
    return out;
}

static VkCullModeFlags getCullModeFlags(gfx::CullMode mode)
{
    switch (mode) {
        case gfx::CullMode::kCullNone:
            return VK_CULL_MODE_NONE;
        case gfx::CullMode::kCullFront:
            return VK_CULL_MODE_FRONT_BIT;
        case gfx::CullMode::kCullBack:
            return VK_CULL_MODE_BACK_BIT;
        case gfx::CullMode::kCullFrontAndBack:
            return VK_CULL_MODE_FRONT_AND_BACK;
        default:
            throw std::runtime_error("Unknown cull mode");
    }
}

static VkFormat getImageFormat(gfx::ImageFormat format)
{
    switch (format) {
        case gfx::ImageFormat::kLinear:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case gfx::ImageFormat::kSRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        default:
            throw std::runtime_error("Unknown image format");
    }
}

} // namespace converters

// functions

static VkShaderModule compileShader(VkDevice device, shaderc_shader_kind kind, const std::string& source, const char* filename)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
    options.SetAutoBindUniforms(false);

    // preprocess
    shaderc::PreprocessedSourceCompilationResult preprocessed = compiler.PreprocessGlsl(source, kind, filename, options);

    if (preprocessed.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error("PREPROCESS ERR " + preprocessed.GetErrorMessage());
    }

    std::string shaderStr{preprocessed.cbegin(), preprocessed.cend()};

    // compile
    shaderc::SpvCompilationResult compiledShader = compiler.CompileGlslToSpv(shaderStr.c_str(), kind, filename, options);

    if (compiledShader.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error("COMPILE ERR " + compiledShader.GetErrorMessage());
    }

    std::vector<uint32_t> shaderBytecode = {compiledShader.cbegin(), compiledShader.cend()}; // not sure why sample code copy vector like this

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderBytecode.size() * sizeof(uint32_t);
    createInfo.pCode = compiledShader.cbegin();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    [[maybe_unused]] VkResult res;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    VKCHECK(res);

    { // record the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        VKCHECK(res);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        res = vkEndCommandBuffer(commandBuffer);
        VKCHECK(res);
    }

    // submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    VKCHECK(res);

    res = vkQueueWaitIdle(queue);
    VKCHECK(res);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// class definitions

struct GFXDevice::Impl {
    // device settings
    gfx::GraphicsSettings graphicsSettings;

    SDL_Window* window = nullptr;
    Instance instance{};
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Device device{};
    VmaAllocator allocator{};
    SwapchainInfo swapchainInfo{};
    Swapchain swapchain{};

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    struct WriteQueues {
        std::unordered_set<gfx::UniformBuffer*> uniform_buffer_writes{};
    };

    std::array<WriteQueues, FRAMES_IN_FLIGHT> write_queues{};

    // For one-off transfer operations not bound to a specific frame-in-flight
    VkCommandPool transferCommandPool = VK_NULL_HANDLE;
    // For one-off operation on the draw queue family not bound to a specific
    // frame-in-flight
    VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;

    uint64_t FRAMECOUNT = 0;

    FrameData frameData[FRAMES_IN_FLIGHT] = {};

    bool swapchainIsOutOfDate = false;
};

GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window, gfx::GraphicsSettings settings)
{
    pimpl = std::make_unique<Impl>();

    VkResult res;

    pimpl->window = window;
    pimpl->graphicsSettings = settings;

    // initialise vulkan

    res = volkInitialize();
    if (res != VK_SUCCESS) {
        throw std::runtime_error("Unable to load vulkan, is it installed?");
    }

    uint32_t vulkanVersion = volkGetInstanceVersion();
    assert(vulkanVersion != 0);
    if (vulkanVersion < VK_API_VERSION_1_3) {
        throw std::runtime_error("The loaded Vulkan version must be at least 1.3");
    }

    pimpl->instance = createVulkanInstance(pimpl->window, appName, appVersion, pimpl->graphicsSettings.enable_validation, MessageSeverity::SEV_WARNING);

    if (SDL_Vulkan_CreateSurface(pimpl->window, pimpl->instance.instance, &pimpl->surface) == false) {
        throw std::runtime_error("Unable to create window surface");
    };

    DeviceRequirements deviceRequirements{};
    deviceRequirements.requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceRequirements.optionalExtensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
    deviceRequirements.optionalExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    deviceRequirements.requiredFeatures.samplerAnisotropy = VK_TRUE;
    // deviceRequirements.requiredFeatures.fillModeNonSolid = VK_TRUE;
    // extension feature memoryPriority is enabled if extension is specified above
    // synchronization2 is always required as it's part of Vulkan 1.3
    // dynamic_rendering is always required as it's part of Vulkan 1.3
    deviceRequirements.formats.push_back(
        FormatRequirements{.format = VK_FORMAT_R8G8B8A8_SRGB, // 2D textures and cubemaps
                           .properties = VkFormatProperties{
                               .linearTilingFeatures = {},
                               .optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                                        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT,
                               .bufferFeatures = {},
                           }});
    deviceRequirements.formats.push_back(FormatRequirements{.format = VK_FORMAT_R32G32_SFLOAT, // vec2 vertex attribute
                                                            .properties = VkFormatProperties{
                                                                .linearTilingFeatures = {},
                                                                .optimalTilingFeatures = {},
                                                                .bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
                                                            }});
    deviceRequirements.formats.push_back(FormatRequirements{.format = VK_FORMAT_R32G32B32_SFLOAT, // vec3 vertex attribute
                                                            .properties = VkFormatProperties{
                                                                .linearTilingFeatures = {},
                                                                .optimalTilingFeatures = {},
                                                                .bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
                                                            }});
    deviceRequirements.formats.push_back(FormatRequirements{.format = VK_FORMAT_R32G32B32A32_SFLOAT, // vec4 vertex attribute
                                                            .properties = VkFormatProperties{
                                                                .linearTilingFeatures = {},
                                                                .optimalTilingFeatures = {},
                                                                .bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
                                                            }});
    deviceRequirements.formats.push_back(FormatRequirements{.format = VK_FORMAT_D16_UNORM, // depth buffer format
                                                            .properties = VkFormatProperties{
                                                                .linearTilingFeatures = {},
                                                                .optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                                .bufferFeatures = {},
                                                            }});
    // the surface format is found later in createSwapchain();

    pimpl->device = createDevice(pimpl->instance.instance, deviceRequirements, pimpl->surface);

    pimpl->allocator = createAllocator(pimpl->instance.instance, pimpl->device);

    pimpl->swapchainInfo.device = pimpl->device.device;
    pimpl->swapchainInfo.allocator = pimpl->allocator;
    pimpl->swapchainInfo.physicalDevice = pimpl->device.physicalDevice;
    pimpl->swapchainInfo.surface = pimpl->surface;
    pimpl->swapchainInfo.window = pimpl->window;
    pimpl->swapchainInfo.vsync = pimpl->graphicsSettings.vsync;
    pimpl->swapchainInfo.waitForPresent = pimpl->graphicsSettings.wait_for_present;
    createSwapchain(&pimpl->swapchain, pimpl->swapchainInfo);

    /* make synchronisation primitives for rendering and allocate command buffers
     */

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        res = vkCreateFence(pimpl->device.device, &fenceInfo, nullptr, &pimpl->frameData[i].renderFence);
        if (res != VK_SUCCESS) throw std::runtime_error("Failed to create fence!");

        VkSemaphoreCreateInfo smphInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0};
        VKCHECK(vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].transferSemaphore));
        VKCHECK(vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].renderSemaphore));
        VKCHECK(vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].presentSemaphore));

        VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .pNext = nullptr,
                                         .flags = 0, // Command buffers cannot be individually reset (more
                                                     // performant this way)
                                         .queueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily};
        VKCHECK(vkCreateCommandPool(pimpl->device.device, &poolInfo, nullptr, &pimpl->frameData[i].graphicsPool));
        poolInfo.queueFamilyIndex = pimpl->device.queues.transferQueueFamily;
        VKCHECK(vkCreateCommandPool(pimpl->device.device, &poolInfo, nullptr, &pimpl->frameData[i].transferPool));

        VkCommandBufferAllocateInfo cmdAllocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                 .pNext = nullptr,
                                                 .commandPool = pimpl->frameData[i].graphicsPool,
                                                 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                 .commandBufferCount = 1};
        VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &cmdAllocInfo, &pimpl->frameData[i].drawBuf));
        cmdAllocInfo.commandPool = pimpl->frameData[i].transferPool;
        VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &cmdAllocInfo, &pimpl->frameData[i].transferBuf));
    }

    /* create command pool for one-off transfer operations */
    VkCommandPoolCreateInfo transferPoolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                             .pNext = nullptr,
                                             .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, // These command buffers
                                                                                            // don't last very long
                                             .queueFamilyIndex = pimpl->device.queues.transferQueueFamily};
    VKCHECK(vkCreateCommandPool(pimpl->device.device, &transferPoolInfo, nullptr, &pimpl->transferCommandPool));
    /* create command pool for one-off draw queue operations */
    VkCommandPoolCreateInfo graphicsPoolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                             .pNext = nullptr,
                                             .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, // These command buffers
                                                                                            // don't last very long
                                             .queueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily};
    VKCHECK(vkCreateCommandPool(pimpl->device.device, &graphicsPoolInfo, nullptr, &pimpl->graphicsCommandPool));

    /* create a global descriptor pool */
    std::vector<VkDescriptorPoolSize> poolSizes{};
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100u});
    poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100u});

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.maxSets = 10000u;
    descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    VKCHECK(vkCreateDescriptorPool(pimpl->device.device, &descriptorPoolInfo, nullptr, &pimpl->descriptorPool));
}

GFXDevice::~GFXDevice()
{
    vkDestroyDescriptorPool(pimpl->device.device, pimpl->descriptorPool, nullptr);

    vkDestroyCommandPool(pimpl->device.device, pimpl->graphicsCommandPool, nullptr);
    vkDestroyCommandPool(pimpl->device.device, pimpl->transferCommandPool, nullptr);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(pimpl->device.device, pimpl->frameData[i].transferPool, nullptr);
        vkDestroyCommandPool(pimpl->device.device, pimpl->frameData[i].graphicsPool, nullptr);
        vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].presentSemaphore, nullptr);
        vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].renderSemaphore, nullptr);
        vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].transferSemaphore, nullptr);
        vkDestroyFence(pimpl->device.device, pimpl->frameData[i].renderFence, nullptr);
    }

    destroySwapchain(pimpl->swapchain);
    destroyAllocator(pimpl->allocator);
    destroyDevice(pimpl->device);
    vkDestroySurfaceKHR(pimpl->instance.instance, pimpl->surface, nullptr);
    destroyVulkanInstance(pimpl->instance);
}

void GFXDevice::GetViewportSize(uint32_t* w, uint32_t* h)
{
    int width, height;
    SDL_Vulkan_GetDrawableSize(pimpl->window, &width, &height);
    if (width == 0 || height == 0) {
        *w = (uint32_t)pimpl->swapchain.extent.width;
        *h = (uint32_t)pimpl->swapchain.extent.height;
    }
    else {
        *w = (uint32_t)width;
        *h = (uint32_t)height;
    }
}

void GFXDevice::SetupImguiBackend()
{
    auto loaderFunc = [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
        return vkGetInstanceProcAddr(*reinterpret_cast<VkInstance*>(user_data), function_name);
    };
    if (ImGui_ImplVulkan_LoadFunctions(loaderFunc, &pimpl->instance.instance) == false) throw std::runtime_error("Failed to load vulkan functions for imgui");

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &pimpl->swapchain.surfaceFormat.format;
    renderingInfo.depthAttachmentFormat = pimpl->swapchain.depthStencilFormat;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = pimpl->instance.instance;
    initInfo.PhysicalDevice = pimpl->device.physicalDevice;
    initInfo.Device = pimpl->device.device;
    initInfo.QueueFamily = pimpl->device.queues.presentAndDrawQueueFamily;
    initInfo.Queue = pimpl->device.queues.drawQueues.back(); // hopefully this isn't used by anything else?
    initInfo.DescriptorPool = pimpl->descriptorPool;
    initInfo.RenderPass = VK_NULL_HANDLE;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = check_vk_result;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = renderingInfo;
    bool success = ImGui_ImplVulkan_Init(&initInfo);
    if (!success) throw std::runtime_error("ImGui_ImplVulkan_Init failed!");

    ImGui_ImplVulkan_CreateFontsTexture();
}

void GFXDevice::ShutdownImguiBackend()
{
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
}

void GFXDevice::CmdRenderImguiDrawData(gfx::DrawBuffer* draw_buffer, ImDrawData* draw_data)
{
    ImGui_ImplVulkan_RenderDrawData(draw_data, draw_buffer->frameData.drawBuf);
}

gfx::DrawBuffer* GFXDevice::BeginRender()
{
    VkResult res;

    const uint32_t currentFrameIndex = pimpl->FRAMECOUNT % FRAMES_IN_FLIGHT;
    const FrameData frameData = pimpl->frameData[currentFrameIndex];

    vmaSetCurrentFrameIndex(pimpl->allocator, (uint32_t)pimpl->FRAMECOUNT);

    /* wait until the previous frame RENDERING has finished */
    // this allows for images acquired by vkAcquireNextImageKHR to be used immediately
    res = vkWaitForFences(pimpl->device.device, 1, &frameData.renderFence, VK_TRUE, 1000000000LL);
    VKCHECK(res);
    res = vkResetFences(pimpl->device.device, 1, &frameData.renderFence);
    VKCHECK(res);

    /* perform any pending uniform buffer writes */

    VKCHECK(vkResetCommandPool(pimpl->device.device, frameData.transferPool, 0));
    VkCommandBufferBeginInfo transferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr // ignored
    };
    VKCHECK(vkBeginCommandBuffer(frameData.transferBuf, &transferBeginInfo));

    // transfer cmds...

    // ownership transfer isn't needed since the data isn't accessed it's just overwritten

    {
        // copy staging buffers to GPU buffers
        for (gfx::UniformBuffer* uniformBuffer : pimpl->write_queues[currentFrameIndex].uniform_buffer_writes) {
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;
            copyRegion.dstOffset = 0;
            copyRegion.size = uniformBuffer->stagingBuffer.size;
            vkCmdCopyBuffer(frameData.transferBuf, uniformBuffer->stagingBuffer.buffer, uniformBuffer->gpuBuffers[currentFrameIndex].buffer, 1, &copyRegion);
        }
    }

    {
        // release buffers to graphics queue
        std::vector<VkBufferMemoryBarrier2> releaseBarriers{};
        for (gfx::UniformBuffer* uniformBuffer : pimpl->write_queues[currentFrameIndex].uniform_buffer_writes) {
            VkBufferMemoryBarrier2& barrier = releaseBarriers.emplace_back();
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
            barrier.dstAccessMask = 0;
            barrier.srcQueueFamilyIndex = pimpl->device.queues.transferQueueFamily;
            barrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            barrier.buffer = uniformBuffer->gpuBuffers[currentFrameIndex].buffer;
            barrier.offset = 0;
            barrier.size = uniformBuffer->gpuBuffers[currentFrameIndex].size;
        }
        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = (uint32_t)releaseBarriers.size();
        dependencyInfo.pBufferMemoryBarriers = releaseBarriers.data();
        vkCmdPipelineBarrier2(frameData.transferBuf, &dependencyInfo);
    }

    VKCHECK(vkEndCommandBuffer(frameData.transferBuf));
    VkSubmitInfo transferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0, // needs to wait for render but the fence does that
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &frameData.transferBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frameData.transferSemaphore,
    };
    res = vkQueueSubmit(pimpl->device.queues.transferQueues[0], 1, &transferSubmitInfo, VK_NULL_HANDLE);
    VKCHECK(res);

    uint32_t swapchainImageIndex;

    do {
        if (pimpl->swapchainIsOutOfDate) {
            // re-create swapchain
            vkQueueWaitIdle(pimpl->device.queues.drawQueues[0]);
            vkQueueWaitIdle(pimpl->device.queues.presentQueue);
            createSwapchain(&pimpl->swapchain, pimpl->swapchainInfo);
        }
        // THIS FUNCTION BLOCKS UNTIL AN IMAGE IS AVAILABLE (it waits for vsync)
        res = vkAcquireNextImageKHR(pimpl->device.device, pimpl->swapchain.swapchain, 1000000000LL, frameData.presentSemaphore, VK_NULL_HANDLE,
                                    &swapchainImageIndex);
        if (res != VK_SUBOPTIMAL_KHR && res != VK_ERROR_OUT_OF_DATE_KHR) VKCHECK(res);
        if (res == VK_SUCCESS) pimpl->swapchainIsOutOfDate = false;
    } while (pimpl->swapchainIsOutOfDate);

    /* record command buffer */
    res = vkResetCommandPool(pimpl->device.device, frameData.graphicsPool, 0);
    VKCHECK(res);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr // ignored
    };
    res = vkBeginCommandBuffer(frameData.drawBuf, &beginInfo);
    VKCHECK(res);

    { // RECORDING, this is executed after the uniform write semaphore is signalled

        // acquire ownership of uniform buffers
        std::vector<VkBufferMemoryBarrier2> acquireBarriers{};
        for (gfx::UniformBuffer* uniformBuffer : pimpl->write_queues[currentFrameIndex].uniform_buffer_writes) {
            VkBufferMemoryBarrier2& barrier = acquireBarriers.emplace_back();
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT;
            barrier.srcQueueFamilyIndex = pimpl->device.queues.transferQueueFamily;
            barrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            barrier.buffer = uniformBuffer->gpuBuffers[currentFrameIndex].buffer;
            barrier.offset = 0;
            barrier.size = uniformBuffer->gpuBuffers[currentFrameIndex].size;
        }
        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = (uint32_t)acquireBarriers.size();
        dependencyInfo.pBufferMemoryBarriers = acquireBarriers.data();
        vkCmdPipelineBarrier2(frameData.drawBuf, &dependencyInfo);

        VkImageMemoryBarrier2 colorImageBarrier{};
        colorImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        colorImageBarrier.pNext = nullptr;
        colorImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorImageBarrier.srcAccessMask = 0;
        colorImageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        colorImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorImageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorImageBarrier.image = pimpl->swapchain.swapchainImages[swapchainImageIndex].first;
        VkImageSubresourceRange colorImageRange{};
        colorImageRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageRange.baseMipLevel = 0;
        colorImageRange.levelCount = 1;
        colorImageRange.baseArrayLayer = 0;
        colorImageRange.layerCount = 1;
        colorImageBarrier.subresourceRange = colorImageRange;

        VkImageMemoryBarrier2 depthImageBarrier{};
        depthImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        depthImageBarrier.pNext = nullptr;
        depthImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        depthImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        depthImageBarrier.srcAccessMask = 0;
        depthImageBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthImageBarrier.image = pimpl->swapchain.depthImages[swapchainImageIndex].image;
        VkImageSubresourceRange depthImageRange{};
        depthImageRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        depthImageRange.baseMipLevel = 0;
        depthImageRange.levelCount = 1;
        depthImageRange.baseArrayLayer = 0;
        depthImageRange.layerCount = 1;
        depthImageBarrier.subresourceRange = depthImageRange;

        std::array<VkImageMemoryBarrier2, 2> imageBarriers{colorImageBarrier, depthImageBarrier};
        VkDependencyInfo imageDependencyInfo{};
        imageDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        imageDependencyInfo.imageMemoryBarrierCount = imageBarriers.size();
        imageDependencyInfo.pImageMemoryBarriers = imageBarriers.data();
        vkCmdPipelineBarrier2(frameData.drawBuf, &imageDependencyInfo);

        std::array<VkClearValue, 2> clearValues{}; // Using same value for all components enables
                                                   // compression according to NVIDIA Best Practices
        clearValues[0].color.float32[0] = 0.0f;
        clearValues[0].color.float32[1] = 0.0f;
        clearValues[0].color.float32[2] = 0.0f;
        clearValues[0].color.float32[3] = 1.0f;
        clearValues[1].depthStencil.depth = 1.0f;

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.pNext = nullptr;
        colorAttachment.imageView = pimpl->swapchain.swapchainImages[swapchainImageIndex].second;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearValues[0];
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.pNext = nullptr;
        depthAttachment.imageView = pimpl->swapchain.depthImages[swapchainImageIndex].view;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue = clearValues[1];

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.pNext = nullptr;
        renderingInfo.flags = 0;
        renderingInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{pimpl->swapchain.extent.width, pimpl->swapchain.extent.height}};
        renderingInfo.layerCount = 1;
        renderingInfo.viewMask = 0;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;
        renderingInfo.pStencilAttachment = nullptr;
        vkCmdBeginRendering(frameData.drawBuf, &renderingInfo);

        VkViewport viewport{};
        if (flip_viewport) {
            viewport.x = 0.0f;
            viewport.y = (float)pimpl->swapchain.extent.height;
            viewport.width = (float)pimpl->swapchain.extent.width;
            viewport.height = -(float)pimpl->swapchain.extent.height;
        }
        else {
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)pimpl->swapchain.extent.width;
            viewport.height = (float)pimpl->swapchain.extent.height;
        }
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(frameData.drawBuf, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = pimpl->swapchain.extent;
        vkCmdSetScissor(frameData.drawBuf, 0, 1, &scissor);
    }

    // clear write queue
    pimpl->write_queues[currentFrameIndex].uniform_buffer_writes.clear();

    // hand command buffer over to caller
    gfx::DrawBuffer* drawBuffer = new gfx::DrawBuffer; // heap allocation every frame but it's only 72 bytes
    drawBuffer->frameData = frameData;
    drawBuffer->currentFrameIndex = currentFrameIndex;
    drawBuffer->imageIndex = swapchainImageIndex;
    return drawBuffer;
}

void GFXDevice::FinishRender(gfx::DrawBuffer* drawBuffer)
{
    assert(drawBuffer != nullptr);

    uint32_t swapchainImageIndex = drawBuffer->imageIndex;
    VkResult res;

    vkCmdEndRendering(drawBuffer->frameData.drawBuf);

#if 0
    // transfer ownership of uniform buffers back to transfer queue
    std::vector<VkBufferMemoryBarrier2> releaseBarriers{};
    for (gfx::UniformBuffer* uniformBuffer : pimpl->write_queues[drawBuffer->currentFrameIndex].uniform_buffer_writes) {
        VkBufferMemoryBarrier2& barrier = releaseBarriers.emplace_back();
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        barrier.dstAccessMask = 0;
        barrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        barrier.dstQueueFamilyIndex = pimpl->device.queues.transferQueueFamily;
        barrier.buffer = uniformBuffer->gpuBuffers[drawBuffer->currentFrameIndex].buffer;
        barrier.offset = 0;
        barrier.size = uniformBuffer->gpuBuffers[drawBuffer->currentFrameIndex].size;
    }
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = (uint32_t)releaseBarriers.size();
    dependencyInfo.pBufferMemoryBarriers = releaseBarriers.data();
    vkCmdPipelineBarrier2(drawBuffer->frameData.drawBuf, &dependencyInfo);
#endif

    // Make color attachment presentable.
    // The present and draw queues are part of the same family so no ownership transfer needed
    VkImageMemoryBarrier2 colorImageBarrier{};
    colorImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    colorImageBarrier.pNext = nullptr;
    colorImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // semaphore takes care of this
    colorImageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    colorImageBarrier.dstAccessMask = 0;
    colorImageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorImageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorImageBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
    colorImageBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
    colorImageBarrier.image = pimpl->swapchain.swapchainImages[swapchainImageIndex].first;
    VkImageSubresourceRange colorImageRange{};
    colorImageRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageRange.baseMipLevel = 0;
    colorImageRange.levelCount = 1;
    colorImageRange.baseArrayLayer = 0;
    colorImageRange.layerCount = 1;
    colorImageBarrier.subresourceRange = colorImageRange;

    VkDependencyInfo imageDependencyInfo{};
    imageDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    imageDependencyInfo.imageMemoryBarrierCount = 1;
    imageDependencyInfo.pImageMemoryBarriers = &colorImageBarrier;
    vkCmdPipelineBarrier2(drawBuffer->frameData.drawBuf, &imageDependencyInfo);

    res = vkEndCommandBuffer(drawBuffer->frameData.drawBuf);
    VKCHECK(res);

    // SUBMIT

    std::vector<VkSemaphore> waitSemaphores{};
    std::vector<VkPipelineStageFlags> waitDstStageMasks{};

    waitSemaphores.push_back(drawBuffer->frameData.presentSemaphore);  // wait for image from 2nd last frame to be presented so it can be rendered to again
    waitDstStageMasks.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    waitSemaphores.push_back(drawBuffer->frameData.transferSemaphore); // wait for uniform buffer copies to complete
    waitDstStageMasks.push_back(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = (uint32_t)waitSemaphores.size(),
        .pWaitSemaphores = waitSemaphores.data(),
        .pWaitDstStageMask = waitDstStageMasks.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &drawBuffer->frameData.drawBuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &drawBuffer->frameData.renderSemaphore,
    };
    res = vkQueueSubmit(pimpl->device.queues.drawQueues[0], 1, &submitInfo, drawBuffer->frameData.renderFence);
    VKCHECK(res);

    // PRESENT

    VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                 .pNext = nullptr,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores = &drawBuffer->frameData.renderSemaphore,
                                 .swapchainCount = 1,
                                 .pSwapchains = &pimpl->swapchain.swapchain,
                                 .pImageIndices = &swapchainImageIndex,
                                 .pResults = nullptr};
    res = vkQueuePresentKHR(pimpl->device.queues.presentQueue, &presentInfo);
    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
        // flag to re-create the swapchain before next render
        pimpl->swapchainIsOutOfDate = true;
    }
    else if (res != VK_SUCCESS)
        throw std::runtime_error("Failed to queue present! Code: " + std::to_string(res));

    pimpl->FRAMECOUNT++;

    delete drawBuffer;
}

gfx::Image* GFXDevice::CreateShadowmapImage()
{
    if (pimpl->FRAMECOUNT != 0) abort();

    gfx::Image* out = new gfx::Image{};

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = pimpl->swapchain.depthStencilFormat;
    imageInfo.extent.width = kShadowmapSize;
    imageInfo.extent.height = kShadowmapSize;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.priority = 0.5f;

    VKCHECK(vmaCreateImage(pimpl->allocator, &imageInfo, &allocCreateInfo, &out->image, &out->allocation, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = pimpl->swapchain.depthStencilFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VKCHECK(vkCreateImageView(pimpl->device.device, &viewInfo, nullptr, &out->view));

    return out;
}

gfx::DrawBuffer* GFXDevice::BeginShadowmapRender(gfx::Image* image)
{
    assert(image != nullptr);
    if (pimpl->FRAMECOUNT != 0) throw std::runtime_error("Can only create shadowmap before proper rendering begins.");

    VkResult res;

    /* record command buffer */
    res = vkResetCommandPool(pimpl->device.device, pimpl->graphicsCommandPool, 0);
    VKCHECK(res);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr // ignored
    };
    res = vkBeginCommandBuffer(pimpl->frameData[0].drawBuf, &beginInfo);
    VKCHECK(res);

    {
        // make the depth image an attachment layout
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.pNext = nullptr;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_NONE;
        barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        barrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        barrier.image = image->image;
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        barrier.subresourceRange = range;

        VkDependencyInfo imageDependencyInfo{};
        imageDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        imageDependencyInfo.imageMemoryBarrierCount = 1;
        imageDependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(pimpl->frameData[0].drawBuf, &imageDependencyInfo);

        VkClearValue clearValue{};
        clearValue.depthStencil.depth = 1.0f;

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.pNext = nullptr;
        depthAttachment.imageView = image->view;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearValue;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.pNext = nullptr;
        renderingInfo.flags = 0;
        renderingInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{kShadowmapSize, kShadowmapSize}};
        renderingInfo.layerCount = 1;
        renderingInfo.viewMask = 0;
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pColorAttachments = nullptr; // no color attachment for this render pass
        renderingInfo.pDepthAttachment = &depthAttachment;
        renderingInfo.pStencilAttachment = nullptr;
        vkCmdBeginRendering(pimpl->frameData[0].drawBuf, &renderingInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)kShadowmapSize;
        viewport.height = (float)kShadowmapSize;

        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(pimpl->frameData[0].drawBuf, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {kShadowmapSize, kShadowmapSize};
        vkCmdSetScissor(pimpl->frameData[0].drawBuf, 0, 1, &scissor);

        // Depth bias (and slope) are used to avoid shadowing artifacts
        // Constant depth bias factor (always applied)
        constexpr float depthBiasConstant = 1.25f;
        // Slope depth bias factor, applied depending on polygon's slope
        constexpr float depthBiasSlope = 1.75f;
        // Set depth bias (aka "Polygon offset")
        // Required to avoid shadow mapping artifacts
        vkCmdSetDepthBias(pimpl->frameData[0].drawBuf, depthBiasConstant, 0.0f, depthBiasSlope);
    }

    // hand command buffer over to caller
    gfx::DrawBuffer* drawBuffer = new gfx::DrawBuffer; // heap allocation every frame but it's only 72 bytes
    drawBuffer->frameData = pimpl->frameData[0];
    drawBuffer->currentFrameIndex = 0;
    drawBuffer->imageIndex = std::numeric_limits<uint32_t>::max(); // meaningless here
    return drawBuffer;
}

void GFXDevice::FinishShadowmapRender(gfx::DrawBuffer* draw_buffer, gfx::Image* image)
{
    assert(draw_buffer != nullptr);

    VkResult res;

    vkCmdEndRendering(draw_buffer->frameData.drawBuf);

    // make the depth image readable by a sampler
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.pNext = nullptr;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
    barrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
    barrier.image = image->image;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    barrier.subresourceRange = range;

    VkDependencyInfo imageDependencyInfo{};
    imageDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    imageDependencyInfo.imageMemoryBarrierCount = 1;
    imageDependencyInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(draw_buffer->frameData.drawBuf, &imageDependencyInfo);

    res = vkEndCommandBuffer(draw_buffer->frameData.drawBuf);
    VKCHECK(res);

    // SUBMIT

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &draw_buffer->frameData.drawBuf,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    res = vkQueueSubmit(pimpl->device.queues.drawQueues[0], 1, &submitInfo, VK_NULL_HANDLE);
    VKCHECK(res);

    vkQueueWaitIdle(pimpl->device.queues.drawQueues[0]);

    delete draw_buffer;
}

void GFXDevice::CmdBindPipeline(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline)
{
    assert(drawBuffer != nullptr);
    assert(pipeline != nullptr);
    vkCmdBindPipeline(drawBuffer->frameData.drawBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
}

void GFXDevice::CmdBindVertexBuffer(gfx::DrawBuffer* drawBuffer, uint32_t binding, const gfx::Buffer* buffer)
{
    assert(drawBuffer != nullptr);
    assert(buffer != nullptr);
    assert(buffer->type == gfx::BufferType::kVertex);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(drawBuffer->frameData.drawBuf, binding, 1, &buffer->buffer, &offset);
}

void GFXDevice::CmdBindIndexBuffer(gfx::DrawBuffer* drawBuffer, const gfx::Buffer* buffer)
{
    assert(drawBuffer != nullptr);
    assert(buffer != nullptr);
    assert(buffer->type == gfx::BufferType::kIndex);
    vkCmdBindIndexBuffer(drawBuffer->frameData.drawBuf, buffer->buffer, 0, INDEX_TYPE);
}

void GFXDevice::CmdDrawIndexed(gfx::DrawBuffer* drawBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                               uint32_t firstInstance)
{
    assert(drawBuffer != nullptr);
    vkCmdDrawIndexed(drawBuffer->frameData.drawBuf, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void GFXDevice::CmdDraw(gfx::DrawBuffer* drawBuffer, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
    assert(drawBuffer != nullptr);
    vkCmdDraw(drawBuffer->frameData.drawBuf, vertex_count, instance_count, first_vertex, first_instance);
}

void GFXDevice::CmdPushConstants(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline, uint32_t offset, uint32_t size, const void* data)
{
    assert(drawBuffer != nullptr);
    assert(pipeline != nullptr);
    assert(data != nullptr);
    vkCmdPushConstants(drawBuffer->frameData.drawBuf, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, offset, size, data);
}

void GFXDevice::CmdBindDescriptorSet(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline, const gfx::DescriptorSet* set, uint32_t setNumber)
{
    assert(drawBuffer != nullptr);
    assert(pipeline != nullptr);
    assert(set != nullptr);
    vkCmdBindDescriptorSets(drawBuffer->frameData.drawBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, setNumber, 1,
                            &set->sets[drawBuffer->currentFrameIndex], 0, nullptr);
}

gfx::Pipeline* GFXDevice::CreatePipeline(const gfx::PipelineInfo& info)
{
    [[maybe_unused]] VkResult res;

    gfx::Pipeline* pipeline = new gfx::Pipeline;

    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    // be careful with these .c_str() calls. It is OK here because 'info' exists for the duration of CreatePipeline()
    {
        auto vertShaderCode = util::ReadTextFile(info.vert_shader_path.c_str());
        vertShaderModule = compileShader(pimpl->device.device, shaderc_vertex_shader, vertShaderCode->data(), info.vert_shader_path.c_str());
    }
    {
        auto fragShaderCode = util::ReadTextFile(info.frag_shader_path.c_str());
        fragShaderModule = compileShader(pimpl->device.device, shaderc_fragment_shader, fragShaderCode->data(), info.frag_shader_path.c_str());
    }

    // get vertex attrib layout:
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = info.vertex_format.stride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribDescs{};
    attribDescs.reserve(info.vertex_format.attribute_descriptions.size());
    for (const auto& desc : info.vertex_format.attribute_descriptions) {
        VkVertexInputAttributeDescription vulkanAttribDesc{};
        vulkanAttribDesc.location = desc.location;
        vulkanAttribDesc.binding = 0;
        vulkanAttribDesc.format = converters::getVertexAttribFormat(desc.format);
        vulkanAttribDesc.offset = desc.offset;
        attribDescs.push_back(vulkanAttribDesc);
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {vertShaderStageInfo, fragShaderStageInfo};

    // set the vertex input layout
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attribDescs.size();
    vertexInputInfo.pVertexAttributeDescriptions = attribDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = info.line_primitives ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

#if 0
    VkViewport viewport{};
    if (flip_viewport) {
        viewport.x = 0.0f;
        viewport.y = (float)pimpl->swapchain.extent.height;
        viewport.width = (float)pimpl->swapchain.extent.width;
        viewport.height = -(float)pimpl->swapchain.extent.height;
    }
    else {
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)pimpl->swapchain.extent.width;
        viewport.height = (float)pimpl->swapchain.extent.height;
    }
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = pimpl->swapchain.extent;
#endif

    // Dynamic states removes the need to re-create pipelines whenever the window
    // size changes
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE; // enabling this will not run the fragment shaders at all
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = converters::getCullModeFlags(info.face_cull_mode);
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // ignored
    rasterizer.depthBiasClamp = 0.0f;          // ignored
    rasterizer.depthBiasSlopeFactor = 0.0f;    // ignored

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 1.0f;          // ignored
    multisampling.pSampleMask = nullptr;            // ignored
    multisampling.alphaToCoverageEnable = VK_FALSE; // ignored
    multisampling.alphaToOneEnable = VK_FALSE;      // ignored

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (info.alpha_blending) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    else {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // ignored
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // ignored
    colorBlending.blendConstants[1] = 0.0f; // ignored
    colorBlending.blendConstants[2] = 0.0f; // ignored
    colorBlending.blendConstants[3] = 0.0f; // ignored

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = info.write_z ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = PUSH_CONSTANT_MAX_SIZE;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(info.descriptor_set_layouts.size());
    for (size_t i = 0; i < descriptorSetLayouts.size(); i++) {
        descriptorSetLayouts[i] = info.descriptor_set_layouts[i]->layout;
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    res = vkCreatePipelineLayout(pimpl->device.device, &layoutInfo, nullptr, &pipeline->layout);
    assert(res == VK_SUCCESS);

    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.stageCount = 2;
    createInfo.pStages = shaderStages;
    createInfo.pVertexInputState = &vertexInputInfo;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pViewportState = &viewportState;
    createInfo.pRasterizationState = &rasterizer;
    createInfo.pMultisampleState = &multisampling;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pColorBlendState = &colorBlending;
    createInfo.pDynamicState = &dynamicState;
    createInfo.layout = pipeline->layout;
    createInfo.renderPass = VK_NULL_HANDLE;
    createInfo.subpass = 0;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = -1;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    if (info.depth_attachment_only) {
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pColorAttachmentFormats = nullptr;
    }
    else {
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachmentFormats = &pimpl->swapchain.surfaceFormat.format;
    }
    renderingInfo.depthAttachmentFormat = pimpl->swapchain.depthStencilFormat;

    createInfo.pNext = &renderingInfo;

    res = vkCreateGraphicsPipelines(pimpl->device.device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->handle);
    assert(res == VK_SUCCESS);

    vkDestroyShaderModule(pimpl->device.device, fragShaderModule, nullptr);
    vkDestroyShaderModule(pimpl->device.device, vertShaderModule, nullptr);

    return pipeline;
}

void GFXDevice::DestroyPipeline(const gfx::Pipeline* pipeline)
{
    assert(pipeline != nullptr);
    vkDestroyPipeline(pimpl->device.device, pipeline->handle, nullptr);
    vkDestroyPipelineLayout(pimpl->device.device, pipeline->layout, nullptr);
    delete pipeline;
}

gfx::DescriptorSetLayout* GFXDevice::CreateDescriptorSetLayout(const std::vector<gfx::DescriptorSetLayoutBinding>& bindings)
{
    gfx::DescriptorSetLayout* out = new gfx::DescriptorSetLayout{};

    std::vector<VkDescriptorSetLayoutBinding> vulkanBindings{};
    uint32_t i = 0;
    for (const auto& binding : bindings) {
        auto& vulkanBinding = vulkanBindings.emplace_back();
        vulkanBinding.binding = i;         // This should be as low as possible to avoid wasting memory
        vulkanBinding.descriptorType = converters::getDescriptorType(binding.descriptor_type);
        vulkanBinding.descriptorCount = 1; // if > 1, accessible as an array in the shader
        vulkanBinding.stageFlags = converters::getShaderStageFlags(binding.stage_flags);

        ++i;
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.bindingCount = (uint32_t)vulkanBindings.size();
    info.pBindings = vulkanBindings.data();
    VKCHECK(vkCreateDescriptorSetLayout(pimpl->device.device, &info, nullptr, &out->layout));

    return out;
}

void GFXDevice::DestroyDescriptorSetLayout(const gfx::DescriptorSetLayout* layout)
{
    vkDestroyDescriptorSetLayout(pimpl->device.device, layout->layout, nullptr);
    delete layout;
}

gfx::DescriptorSet* GFXDevice::AllocateDescriptorSet(const gfx::DescriptorSetLayout* layout)
{
    assert(layout != nullptr);

    gfx::DescriptorSet* set = new gfx::DescriptorSet{};

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .pNext = nullptr,
                                              .descriptorPool = pimpl->descriptorPool,
                                              .descriptorSetCount = 1,
                                              .pSetLayouts = &layout->layout};
        VkResult res;
        res = vkAllocateDescriptorSets(pimpl->device.device, &allocInfo, &set->sets[i]);
        if (res == VK_ERROR_FRAGMENTED_POOL) throw std::runtime_error("Descriptor pool is fragmented!");
        if (res == VK_ERROR_OUT_OF_POOL_MEMORY) throw std::runtime_error("Descriptor pool is out of memory!");
        VKCHECK(res);
    }

    return set;
}

void GFXDevice::FreeDescriptorSet(const gfx::DescriptorSet* set)
{
    assert(set != nullptr);
    VKCHECK(vkFreeDescriptorSets(pimpl->device.device, pimpl->descriptorPool, static_cast<uint32_t>(set->sets.size()), set->sets.data()));
}

void GFXDevice::UpdateDescriptorUniformBuffer(const gfx::DescriptorSet* set, uint32_t binding, const gfx::UniformBuffer* buffer, size_t offset, size_t range)
{
    assert(set != nullptr);
    assert(buffer != nullptr);

    assert(pimpl->FRAMECOUNT == 0);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{.buffer = buffer->gpuBuffers[i].buffer, .offset = offset, .range = range};
        VkWriteDescriptorSet descriptorWrite{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                             .pNext = nullptr,
                                             .dstSet = set->sets[i],
                                             .dstBinding = binding,
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             .pImageInfo = nullptr,
                                             .pBufferInfo = &bufferInfo,
                                             .pTexelBufferView = nullptr};
        vkUpdateDescriptorSets(pimpl->device.device, 1, &descriptorWrite, 0, nullptr);
    }
}

void GFXDevice::UpdateDescriptorCombinedImageSampler(const gfx::DescriptorSet* set, uint32_t binding, const gfx::Image* image, const gfx::Sampler* sampler)
{
    assert(set != nullptr);
    assert(image != nullptr);
    assert(sampler != nullptr);

    if (pimpl->FRAMECOUNT != 0) abort(); // TODO. This is annoying

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler->sampler;
    imageInfo.imageView = image->view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkWriteDescriptorSet descriptorWrite{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                             .pNext = nullptr,
                                             .dstSet = set->sets[i],
                                             .dstBinding = binding,
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                             .pImageInfo = &imageInfo,
                                             .pBufferInfo = nullptr,
                                             .pTexelBufferView = nullptr};
        vkUpdateDescriptorSets(pimpl->device.device, 1, &descriptorWrite, 0, nullptr);
    }
}

gfx::UniformBuffer* GFXDevice::CreateUniformBuffer(uint64_t size, const void* initialData)
{
    assert(initialData != nullptr);

    gfx::UniformBuffer* out = new gfx::UniformBuffer{};

    /* first make staging buffer */
    out->stagingBuffer.size = size;
    out->stagingBuffer.type = gfx::BufferType::kUniform;
    {
        VkBufferCreateInfo stagingBufferInfo{};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = out->stagingBuffer.size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        stagingBufferInfo.flags = 0;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VKCHECK(vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &out->stagingBuffer.buffer, &out->stagingBuffer.allocation, nullptr));

        void* dataDest;
        VKCHECK(vmaMapMemory(pimpl->allocator, out->stagingBuffer.allocation, &dataDest));
        memcpy(dataDest, initialData, out->stagingBuffer.size);
        vmaUnmapMemory(pimpl->allocator, out->stagingBuffer.allocation);
    }

    /* create the device-local set of buffers */
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        out->gpuBuffers[i].size = out->stagingBuffer.size;
        out->gpuBuffers[i].type = gfx::BufferType::kUniform;

        VkBufferCreateInfo gpuBufferInfo{};
        gpuBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        gpuBufferInfo.size = out->gpuBuffers[i].size;
        gpuBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        gpuBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        gpuBufferInfo.flags = 0;

        VmaAllocationCreateInfo gpuAllocationInfo{};
        gpuAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        gpuAllocationInfo.flags = 0;

        VKCHECK(vmaCreateBuffer(pimpl->allocator, &gpuBufferInfo, &gpuAllocationInfo, &out->gpuBuffers[i].buffer, &out->gpuBuffers[i].allocation, nullptr));

        /* copy staging buffer into both */
        copyBuffer(pimpl->device.device, pimpl->transferCommandPool, pimpl->device.queues.transferQueues[0], out->stagingBuffer.buffer,
                   out->gpuBuffers[i].buffer, out->stagingBuffer.size);
    }

    return out;
}

void GFXDevice::DestroyUniformBuffer(const gfx::UniformBuffer* uniformBuffer)
{
    assert(uniformBuffer != nullptr);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(pimpl->allocator, uniformBuffer->gpuBuffers[i].buffer, uniformBuffer->gpuBuffers[i].allocation);
    }

    vmaDestroyBuffer(pimpl->allocator, uniformBuffer->stagingBuffer.buffer, uniformBuffer->stagingBuffer.allocation);

    delete uniformBuffer;
}

void GFXDevice::WriteUniformBuffer(gfx::UniformBuffer* buffer, uint64_t offset, uint64_t size, const void* data)
{
    assert(buffer != nullptr);
    assert(data != nullptr);

    assert(offset + size <= buffer->stagingBuffer.size);

    /* first update the staging buffer */
    void* dataDest;
    VKCHECK(vmaMapMemory(pimpl->allocator, buffer->stagingBuffer.allocation, &dataDest));
    memcpy(dataDest, (uint8_t*)data + offset, size);
    vmaUnmapMemory(pimpl->allocator, buffer->stagingBuffer.allocation);

    /* queue the writes to each gpu buffer */
    // This is required as buffers cannot be updated if they are currently in use
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        pimpl->write_queues[i].uniform_buffer_writes.insert(buffer);
    }
}

gfx::Buffer* GFXDevice::CreateBuffer(gfx::BufferType type, uint64_t size, const void* data)
{
    assert(data != nullptr);

    auto out = new gfx::Buffer{};
    out->size = size;
    out->type = type;

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    // first create the staging buffer
    {
        VkBufferCreateInfo stagingBufferInfo{};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = out->size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        stagingBufferInfo.flags = 0;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VKCHECK(vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr));

        void* dataDest;
        VKCHECK(vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest));
        memcpy(dataDest, data, out->size);
        vmaUnmapMemory(pimpl->allocator, stagingAllocation);
    }

    // create the actual buffer on the GPU
    {
        VkBufferCreateInfo gpuBufferInfo{};
        gpuBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        gpuBufferInfo.size = out->size;
        gpuBufferInfo.usage = converters::getBufferUsageFlag(type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        gpuBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        gpuBufferInfo.flags = 0;

        VmaAllocationCreateInfo gpuAllocationInfo{};
        gpuAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        gpuAllocationInfo.flags = 0;

        VKCHECK(vmaCreateBuffer(pimpl->allocator, &gpuBufferInfo, &gpuAllocationInfo, &out->buffer, &out->allocation, nullptr));
    }

    // copy the data from the staging buffer to the gpu buffer
    copyBuffer(pimpl->device.device, pimpl->transferCommandPool, pimpl->device.queues.transferQueues[0], stagingBuffer, out->buffer, out->size);

    // destroy staging buffer
    vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);
    return out;
}

void GFXDevice::DestroyBuffer(const gfx::Buffer* buffer)
{
    assert(buffer != nullptr);
    vmaDestroyBuffer(pimpl->allocator, buffer->buffer, buffer->allocation);
    delete buffer;
}

// imageData must have pixel format R8G8B8A8
gfx::Image* GFXDevice::CreateImage(uint32_t w, uint32_t h, gfx::ImageFormat input_format, const void* imageData)
{
    assert(imageData != nullptr);

    if (pimpl->FRAMECOUNT != 0) abort(); //  TODO. This is annoying

    gfx::Image* out = new gfx::Image{};

    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
    VkFormat imageFormat = converters::getImageFormat(input_format);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    VkDeviceSize stagingBufferSize = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    /* create staging buffer */
    {
        VkBufferCreateInfo stagingBufferInfo{};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = stagingBufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        stagingBufferInfo.flags = 0;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VKCHECK(vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr));

        void* dataDest;
        VKCHECK(vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest));
        memcpy(dataDest, imageData, stagingBufferSize);
        vmaUnmapMemory(pimpl->allocator, stagingAllocation);
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent.width = w;
    imageInfo.extent.height = h;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.priority = 0.5f;

    VKCHECK(vmaCreateImage(pimpl->allocator, &imageInfo, &allocCreateInfo, &out->image, &out->allocation, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VKCHECK(vkCreateImageView(pimpl->device.device, &viewInfo, nullptr, &out->view));

    /* begin command buffer */
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pimpl->graphicsCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &allocInfo, &commandBuffer));

    { // record the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKCHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        // barrier: (all mip levels): UNDEFINED -> TRANSFER_DST_OPTIMAL
        // Used for copying staging buffer AND blitting mipmaps
        // Must happen before vkCmdCopyBufferToImage performs a TRANSFER_WRITE in
        // the COPY stage.
        VkImageMemoryBarrier2 beforeCopyBarrier{};
        beforeCopyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        beforeCopyBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        beforeCopyBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        beforeCopyBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT; // all subresources will be COPYed or BLITed to next
        beforeCopyBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT; // Image must be TRANSFER_DST_OPTIMAL before either stage performs a TRANSFER_WRITE
        beforeCopyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        beforeCopyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        beforeCopyBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        beforeCopyBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        beforeCopyBarrier.image = out->image;
        beforeCopyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        beforeCopyBarrier.subresourceRange.baseMipLevel = 0;
        beforeCopyBarrier.subresourceRange.levelCount = mipLevels;
        beforeCopyBarrier.subresourceRange.baseArrayLayer = 0;
        beforeCopyBarrier.subresourceRange.layerCount = 1;
        VkDependencyInfo beforeCopyDependency{};
        beforeCopyDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        beforeCopyDependency.imageMemoryBarrierCount = 1;
        beforeCopyDependency.pImageMemoryBarriers = &beforeCopyBarrier;
        vkCmdPipelineBarrier2(commandBuffer, &beforeCopyDependency);

        // copy staging buffer to mipLevel 0 (full res image)
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset.x = 0;
        region.imageOffset.y = 0;
        region.imageOffset.z = 0;
        region.imageExtent = imageInfo.extent;
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        int32_t mipWidth = w;
        int32_t mipHeight = h;
        for (uint32_t i = 1; i < mipLevels; i++) {
            // barrier: (i - 1) TRANSFER_DST_OPTIMAL -> TRANSFER_SRC_OPTIMAL
            // Must happen after TRANSFER_WRITE in the COPY stage and BLIT stage.
            VkImageMemoryBarrier2 beforeBlitBarrier{};
            beforeBlitBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            beforeBlitBarrier.srcStageMask =
                VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;  // previous mip level was either just COPYed to or just BLITed to
            beforeBlitBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT; // these actions were TRANSFER_WRITEs
            beforeBlitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;    // the image will be BLITed from next
            beforeBlitBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;  // this is a TRANSFER_READ
            beforeBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            beforeBlitBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            beforeBlitBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            beforeBlitBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            beforeBlitBarrier.image = out->image;
            beforeBlitBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            beforeBlitBarrier.subresourceRange.baseMipLevel = (i - 1);
            beforeBlitBarrier.subresourceRange.levelCount = 1;
            beforeBlitBarrier.subresourceRange.baseArrayLayer = 0;
            beforeBlitBarrier.subresourceRange.layerCount = 1;
            VkDependencyInfo beforeBlitDependency{};
            beforeBlitDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            beforeBlitDependency.imageMemoryBarrierCount = 1;
            beforeBlitDependency.pImageMemoryBarriers = &beforeBlitBarrier;
            vkCmdPipelineBarrier2(commandBuffer, &beforeBlitDependency);

            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
            vkCmdBlitImage(commandBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                           VK_FILTER_LINEAR);

            // barrier: (i - 1) TRANSFER_SRC_OPTIMAL -> SHADER_READ_ONLY_OPTIMALs
            // Must happen after usage in the BLIT stage.
            // Must happen before SHADER_SAMPLED_READ in the FRAGMENT_SHADER stage
            VkImageMemoryBarrier2 afterBlitBarrier{};
            afterBlitBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            afterBlitBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
            afterBlitBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;       // previous mip level was just READ from in a BLIT
            afterBlitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            afterBlitBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT; // it will next be sampled in a frag shader
            afterBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            afterBlitBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            afterBlitBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            afterBlitBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            afterBlitBarrier.image = out->image;
            afterBlitBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            afterBlitBarrier.subresourceRange.baseMipLevel = (i - 1);
            afterBlitBarrier.subresourceRange.levelCount = 1;
            afterBlitBarrier.subresourceRange.baseArrayLayer = 0;
            afterBlitBarrier.subresourceRange.layerCount = 1;
            VkDependencyInfo afterBlitDependency{};
            afterBlitDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            afterBlitDependency.imageMemoryBarrierCount = 1;
            afterBlitDependency.pImageMemoryBarriers = &afterBlitBarrier;
            vkCmdPipelineBarrier2(commandBuffer, &afterBlitDependency);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 2) mipHeight /= 2;
        }

        // Final mipLevel is never transitioned from TRANSFER_DST_OPTIMAL
        // barrier: (mipLevels - 1) TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier2 finalBlitBarrier{};
        finalBlitBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        if (mipLevels == 1) {
            // if no mipmaps were generated, the image was just COPYed to with a WRITE
            finalBlitBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT; // the final mip level was BLITed to with a WRITE
        }
        else {
            // mips were generated, therefore last mip level was just BLITed to
            finalBlitBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
        }
        finalBlitBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        finalBlitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        finalBlitBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        finalBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        finalBlitBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        finalBlitBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        finalBlitBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        finalBlitBarrier.image = out->image;
        finalBlitBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        finalBlitBarrier.subresourceRange.baseMipLevel = (mipLevels - 1);
        finalBlitBarrier.subresourceRange.levelCount = 1;
        finalBlitBarrier.subresourceRange.baseArrayLayer = 0;
        finalBlitBarrier.subresourceRange.layerCount = 1;
        VkDependencyInfo afterBlitDependency{};
        afterBlitDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        afterBlitDependency.imageMemoryBarrierCount = 1;
        afterBlitDependency.pImageMemoryBarriers = &finalBlitBarrier;
        vkCmdPipelineBarrier2(commandBuffer, &afterBlitDependency);

        VKCHECK(vkEndCommandBuffer(commandBuffer));
    }

    // submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VKCHECK(vkQueueSubmit(pimpl->device.queues.drawQueues[0], 1, &submitInfo, VK_NULL_HANDLE));

    VKCHECK(vkQueueWaitIdle(pimpl->device.queues.drawQueues[0]));

    vkFreeCommandBuffers(pimpl->device.device, pimpl->graphicsCommandPool, 1, &commandBuffer);

    vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);

    return out;
}

gfx::Image* GFXDevice::CreateImageCubemap(uint32_t w, uint32_t h, gfx::ImageFormat input_format, const std::array<const void*, 6>& image_data)
{
    assert(image_data[0] != nullptr);
    assert(image_data[1] != nullptr);
    assert(image_data[2] != nullptr);
    assert(image_data[3] != nullptr);
    assert(image_data[4] != nullptr);
    assert(image_data[5] != nullptr);

    if (pimpl->FRAMECOUNT != 0) abort(); //  TODO. This is annoying

    gfx::Image* out = new gfx::Image{};

    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
    VkFormat imageFormat = converters::getImageFormat(input_format);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    const VkDeviceSize stagingBufferSizePerSide = (VkDeviceSize)w * (VkDeviceSize)h * 4;

    /* create staging buffer */
    {
        VkBufferCreateInfo stagingBufferInfo{};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = stagingBufferSizePerSide * 6;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        stagingBufferInfo.flags = 0;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VKCHECK(vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr));

        void* dataDest;
        VKCHECK(vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest));
        for (size_t i = 0; i < 6; ++i) {
            memcpy(reinterpret_cast<std::byte*>(dataDest) + stagingBufferSizePerSide * i, image_data[i], stagingBufferSizePerSide);
        }
        vmaUnmapMemory(pimpl->allocator, stagingAllocation);
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent.width = w;
    imageInfo.extent.height = h;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.priority = 0.5f;

    VKCHECK(vmaCreateImage(pimpl->allocator, &imageInfo, &allocCreateInfo, &out->image, &out->allocation, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    VKCHECK(vkCreateImageView(pimpl->device.device, &viewInfo, nullptr, &out->view));

    /* begin command buffer */
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pimpl->graphicsCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &allocInfo, &commandBuffer));

    { // record the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VKCHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        // barrier: (all mip levels, all faces): UNDEFINED -> TRANSFER_DST_OPTIMAL
        // Used for copying staging buffer AND blitting mipmaps
        // Must happen before vkCmdCopyBufferToImage performs a TRANSFER_WRITE in
        // the COPY stage.
        VkImageMemoryBarrier2 beforeCopyBarrier{};
        beforeCopyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        beforeCopyBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        beforeCopyBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        beforeCopyBarrier.dstStageMask =
            VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT; // all parts of the image will be either TRANSFERed to or BLITed to next
        beforeCopyBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        beforeCopyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        beforeCopyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        beforeCopyBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        beforeCopyBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
        beforeCopyBarrier.image = out->image;
        beforeCopyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        beforeCopyBarrier.subresourceRange.baseMipLevel = 0;
        beforeCopyBarrier.subresourceRange.levelCount = mipLevels;
        beforeCopyBarrier.subresourceRange.baseArrayLayer = 0;
        beforeCopyBarrier.subresourceRange.layerCount = 6;
        VkDependencyInfo beforeCopyDependency{};
        beforeCopyDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        beforeCopyDependency.imageMemoryBarrierCount = 1;
        beforeCopyDependency.pImageMemoryBarriers = &beforeCopyBarrier;
        vkCmdPipelineBarrier2(commandBuffer, &beforeCopyDependency);

        for (int face = 0; face < 6; ++face) {
            // copy staging buffer to mipLevel 0 (full res image)
            VkBufferImageCopy region{};
            region.bufferOffset = stagingBufferSizePerSide * face;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageOffset.x = 0;
            region.imageOffset.y = 0;
            region.imageOffset.z = 0;
            region.imageExtent = imageInfo.extent;
            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            int32_t mipWidth = w;
            int32_t mipHeight = h;
            for (uint32_t i = 1; i < mipLevels; i++) {
                // barrier: (i - 1) TRANSFER_DST_OPTIMAL -> TRANSFER_SRC_OPTIMAL
                // Must happen after TRANSFER_WRITE in the COPY stage and BLIT stage.
                VkImageMemoryBarrier2 beforeBlitBarrier{};
                beforeBlitBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                beforeBlitBarrier.srcStageMask =
                    VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT; // subresource was either just copied to or just blitted to
                beforeBlitBarrier.srcAccessMask =
                    VK_ACCESS_2_TRANSFER_WRITE_BIT; // wait until CopyBufferToImage and BlitImage have performed TRANSFER_WRITE on subresource
                beforeBlitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;   // subresource is going to be blitted from
                beforeBlitBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT; // subresource will be READ from during the blit
                beforeBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                beforeBlitBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                beforeBlitBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
                beforeBlitBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
                beforeBlitBarrier.image = out->image;
                beforeBlitBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                beforeBlitBarrier.subresourceRange.baseMipLevel = (i - 1);
                beforeBlitBarrier.subresourceRange.levelCount = 1;
                beforeBlitBarrier.subresourceRange.baseArrayLayer = face;
                beforeBlitBarrier.subresourceRange.layerCount = 1;
                VkDependencyInfo beforeBlitDependency{};
                beforeBlitDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                beforeBlitDependency.imageMemoryBarrierCount = 1;
                beforeBlitDependency.pImageMemoryBarriers = &beforeBlitBarrier;
                vkCmdPipelineBarrier2(commandBuffer, &beforeBlitDependency);

                VkImageBlit blit{};
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = face;
                blit.srcSubresource.layerCount = 1;
                blit.srcOffsets[0] = {0, 0, 0};
                blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = face;
                blit.dstSubresource.layerCount = 1;
                blit.dstOffsets[0] = {0, 0, 0};
                blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
                vkCmdBlitImage(commandBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                               VK_FILTER_LINEAR); // WRITE-AFTER-WRITE error here

                // barrier: (i - 1) TRANSFER_SRC_OPTIMAL -> SHADER_READ_ONLY_OPTIMALs
                // Must happen after usage in the BLIT stage.
                // Must happen before SHADER_SAMPLED_READ in the FRAGMENT_SHADER stage
                VkImageMemoryBarrier2 afterBlitBarrier{};
                afterBlitBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                afterBlitBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
                afterBlitBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                afterBlitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                afterBlitBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
                afterBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                afterBlitBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                afterBlitBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
                afterBlitBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
                afterBlitBarrier.image = out->image;
                afterBlitBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                afterBlitBarrier.subresourceRange.baseMipLevel = (i - 1);
                afterBlitBarrier.subresourceRange.levelCount = 1;
                afterBlitBarrier.subresourceRange.baseArrayLayer = face;
                afterBlitBarrier.subresourceRange.layerCount = 1;
                VkDependencyInfo afterBlitDependency{};
                afterBlitDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                afterBlitDependency.imageMemoryBarrierCount = 1;
                afterBlitDependency.pImageMemoryBarriers = &afterBlitBarrier;
                vkCmdPipelineBarrier2(commandBuffer, &afterBlitDependency);

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 2) mipHeight /= 2;
            }

            // Final mipLevel is never transitioned from TRANSFER_DST_OPTIMAL
            // barrier: (mipLevels - 1) TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
            VkImageMemoryBarrier2 finalBlitBarrier{};
            finalBlitBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            finalBlitBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
            finalBlitBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            finalBlitBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            finalBlitBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            finalBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            finalBlitBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            finalBlitBarrier.srcQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            finalBlitBarrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
            finalBlitBarrier.image = out->image;
            finalBlitBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            finalBlitBarrier.subresourceRange.baseMipLevel = (mipLevels - 1);
            finalBlitBarrier.subresourceRange.levelCount = 1;
            finalBlitBarrier.subresourceRange.baseArrayLayer = face;
            finalBlitBarrier.subresourceRange.layerCount = 1;
            VkDependencyInfo afterBlitDependency{};
            afterBlitDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            afterBlitDependency.imageMemoryBarrierCount = 1;
            afterBlitDependency.pImageMemoryBarriers = &finalBlitBarrier;
            vkCmdPipelineBarrier2(commandBuffer, &afterBlitDependency);
        }

        VKCHECK(vkEndCommandBuffer(commandBuffer));
    }

    // submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VKCHECK(vkQueueSubmit(pimpl->device.queues.drawQueues[0], 1, &submitInfo, VK_NULL_HANDLE));

    VKCHECK(vkQueueWaitIdle(pimpl->device.queues.drawQueues[0]));

    vkFreeCommandBuffers(pimpl->device.device, pimpl->graphicsCommandPool, 1, &commandBuffer);

    vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);

    return out;
}

void GFXDevice::DestroyImage(const gfx::Image* image)
{
    assert(image != nullptr);
    vkDestroyImageView(pimpl->device.device, image->view, nullptr);
    vmaDestroyImage(pimpl->allocator, image->image, image->allocation);
    delete image;
}

const gfx::Sampler* GFXDevice::CreateSampler(const gfx::SamplerInfo& info)
{
    gfx::Sampler* out = new gfx::Sampler{};

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = converters::getFilter(info.magnify);
    samplerInfo.minFilter = converters::getFilter(info.minify);
    samplerInfo.mipmapMode = converters::getSamplerMipmapMode(info.mipmap);
    samplerInfo.addressModeU = converters::getSamplerAddressMode(info.wrap_u);
    samplerInfo.addressModeV = converters::getSamplerAddressMode(info.wrap_v);
    samplerInfo.addressModeW = converters::getSamplerAddressMode(info.wrap_w);
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = (info.anisotropic_filtering && pimpl->graphicsSettings.enable_anisotropy) ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = pimpl->device.properties.limits.maxSamplerAnisotropy;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    VKCHECK(vkCreateSampler(pimpl->device.device, &samplerInfo, nullptr, &out->sampler));

    return out;
}

void GFXDevice::DestroySampler(const gfx::Sampler* sampler)
{
    assert(sampler != nullptr);
    vkDestroySampler(pimpl->device.device, sampler->sampler, nullptr);
    delete sampler;
}

void GFXDevice::LogPerformanceInfo()
{
    VmaTotalStatistics pStats{};
    vmaCalculateStatistics(pimpl->allocator, &pStats);

    VkPhysicalDeviceMemoryProperties2 memProps{};
    memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    vkGetPhysicalDeviceMemoryProperties2(pimpl->device.physicalDevice, &memProps);

    LOG_INFO("GPU Memory Statistics:");

    for (uint32_t i = 0; i < memProps.memoryProperties.memoryHeapCount; i++) {
        const VmaStatistics& statistics = pStats.memoryHeap[i].statistics;
        VkMemoryHeap heap = memProps.memoryProperties.memoryHeaps[i];
        LOG_INFO("Memory heap {}", i);
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            LOG_INFO("    DEVICE_LOCAL");
        }
        LOG_INFO("    Memory blocks allocated: {} ({} MiB)", statistics.blockCount, statistics.allocationBytes / (1024 * 1024));
        LOG_INFO("    Number of allocations: {} ({} MiB)", statistics.allocationCount, statistics.allocationBytes / (1024 * 1024));
        LOG_INFO("    Max size: {} MiB", heap.size / (1024 * 1024));
    }
}

uint64_t GFXDevice::GetFrameCount() { return pimpl->FRAMECOUNT; }

void GFXDevice::WaitIdle() { vkDeviceWaitIdle(pimpl->device.device); }

} // namespace engine
