// The implementation of the graphics layer using Vulkan 1.3.

#ifdef ENGINE_BUILD_VULKAN

#include "gfx_device.hpp"
#include "util.hpp"
#include "config.h"
#include "log.hpp"
#include "util/files.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <shaderc/shaderc.hpp>

#include <SDL_vulkan.h>

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>
#include <queue>
#include <map>
#include <iostream>

namespace engine {

	static constexpr uint32_t FRAMES_IN_FLIGHT = 2; // This improved FPS by 5x! (on Intel IGPU)

	static constexpr size_t PUSH_CONSTANT_MAX_SIZE = 128; // bytes

	// structures and enums

	struct LayerInfo {
		std::vector<VkLayerProperties> layersAvailable{};
		std::optional<std::vector<VkLayerProperties>::iterator> validationLayer;
	};

	struct Queue {
		uint32_t familyIndex;
		uint32_t queueIndex;
		bool supportsGraphics;
		bool supportsTransfer;
		bool supportsCompute;

		VkQueue handle;
	};

	struct DepthBuffer {
		VkImage image;
		VmaAllocation allocation;
		VkImageView view;
	};

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;

		VkExtent2D extent;
		VkSurfaceFormatKHR surfaceFormat;
		VkPresentModeKHR presentMode;

		std::vector<VkImage> images{};
		std::vector<VkImageView> imageViews{};
		std::vector<VkFramebuffer> framebuffers{};

		DepthBuffer depthBuffer{};

		// multisampling
		VkSampleCountFlagBits msaaSamples{};
		struct MSTarget {
			VkImage colorImage{};
			VmaAllocation colorImageAllocation{};
			VkImageView colorImageView{};
		} msTarget{};

		VkQueue activeQueue{};

		VkRenderPass renderpass;

		std::array<VkSemaphore, FRAMES_IN_FLIGHT> acquireSemaphores{}; // waits until the image is available
		std::array<VkSemaphore, FRAMES_IN_FLIGHT> releaseSemaphores{}; // waits until rendering finishes
	};

	struct DrawCall {
		const gfx::Pipeline* pipeline = nullptr; // for performance, keep this the same for consecutive draw calls
		const gfx::Buffer* vertexBuffer = nullptr;
		const gfx::Buffer* indexBuffer = nullptr; // if this is nullptr, don't use indexed
		uint32_t count = 0;
		uint8_t pushConstantData[PUSH_CONSTANT_MAX_SIZE];
		const gfx::Texture* texture = nullptr;
	};

	enum class QueueFlags : uint32_t {
		GRAPHICS =	(1 << 0),
		TRANSFER =	(1 << 1),
		COMPUTE =	(1 << 2),
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
		std::vector<gfx::Buffer*> uniformBuffers{};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> descriptorSets{};
	};

	struct gfx::Texture {
		VkImage image;
		VmaAllocation alloc;
		VkImageView imageView;
		VkSampler sampler;
		VkDescriptorPool pool;
		std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> descriptorSets{};
		uint32_t mipLevels;
	};



	// enum converters

	namespace vkinternal {

		static VkFormat getVertexAttribFormat(gfx::VertexAttribFormat fmt)
		{
			switch (fmt) {
			case gfx::VertexAttribFormat::FLOAT2:
				return VK_FORMAT_R32G32_SFLOAT;
			case gfx::VertexAttribFormat::FLOAT3:
				return VK_FORMAT_R32G32B32_SFLOAT;
			case gfx::VertexAttribFormat::FLOAT4:
				return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
			throw std::runtime_error("Unknown vertex attribute format");
		}

		static VkBufferUsageFlagBits getBufferUsageFlag(gfx::BufferType type)
		{
			switch (type) {
			case gfx::BufferType::VERTEX:
				return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case gfx::BufferType::INDEX:
				return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			default:
				throw std::runtime_error("This buffer type does not have usage bits");
			}
		}

		static VkFilter getTextureFilter(gfx::TextureFilter filter)
		{
			switch(filter) {
			case gfx::TextureFilter::LINEAR:
				return VK_FILTER_LINEAR;
			case gfx::TextureFilter::NEAREST:
				return VK_FILTER_NEAREST;
			}
			throw std::runtime_error("Unknown texture filter");
		}

	}

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

		if (preprocessed.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			throw std::runtime_error("PREPROCESS ERR " + preprocessed.GetErrorMessage());
		}

		std::string shaderStr{preprocessed.cbegin(), preprocessed.cend()};

		// compile
		shaderc::SpvCompilationResult compiledShader = compiler.CompileGlslToSpv(shaderStr.c_str(), kind, filename, options);

		if (compiledShader.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			throw std::runtime_error("COMPILE ERR " + compiledShader.GetErrorMessage());
		}

		std::vector<uint32_t> shaderBytecode = { compiledShader.cbegin(), compiledShader.cend() };// not sure why sample code copy vector like this

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderBytecode.size() * sizeof(uint32_t);
		createInfo.pCode = compiledShader.cbegin();

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;

	}

	static std::vector<const char*> getRequiredVulkanExtensions(SDL_Window* window)
	{
		[[maybe_unused]] SDL_bool res;

		unsigned int sdlExtensionCount = 0;
		res = SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr);
		assert(res == SDL_TRUE);
		std::vector<const char*> requiredExtensions(sdlExtensionCount);
		res = SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, requiredExtensions.data());
		assert(res == SDL_TRUE);

		return requiredExtensions;
	}

	static LayerInfo getAvailableLayers(bool useValidation)
	{
		constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

		LayerInfo info;
		[[maybe_unused]] VkResult res;

		uint32_t layerCount;
		res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		assert(res == VK_SUCCESS);
		info.layersAvailable.resize(layerCount);
		res = vkEnumerateInstanceLayerProperties(&layerCount, info.layersAvailable.data());
		assert(res == VK_SUCCESS);

		if (useValidation == true) {
			// find validation layer and print all layers to log
			for (auto it = info.layersAvailable.begin(); it != info.layersAvailable.end(); it++) {
				if (strncmp(it->layerName, VALIDATION_LAYER_NAME, 256) == 0) {
					info.validationLayer = it;
				}
			}
			if (info.validationLayer.has_value() == false) {
				throw std::runtime_error("The validation layer was not found. Quitting.");
			}
		}

		return info;
	}

	static VkBool32 messengerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		(void)pUserData;

		std::string msgType{};

		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			msgType += " (GENERAL)";
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			msgType += " (PERF.)";
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			msgType += " (VALID.)";

		switch (messageSeverity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			DEBUG("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			INFO("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			WARN("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			ERROR("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		default:
			break;
		}
		return VK_FALSE;
	}

	static VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo()
	{
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{
	.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
	.pNext = nullptr,
	.flags = 0,
	.messageSeverity = 0,
	.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
	.pfnUserCallback = messengerCallback,
	.pUserData = nullptr,
		};

		enum class MessageSeverity {
			VERBOSE,
			INFO,
			WARNING,
			ERROR
		};

		constexpr MessageSeverity MESSAGE_LEVEL = MessageSeverity::WARNING;
		switch (MESSAGE_LEVEL) {
		case MessageSeverity::VERBOSE:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
			// fall-through
		case MessageSeverity::INFO:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
			// fall-through
		case MessageSeverity::WARNING:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
			// fall-through
		case MessageSeverity::ERROR:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			// fall-through
		default:
			break;
		}

		return debugMessengerInfo;
	}

	static VkSurfaceKHR createSurface(SDL_Window* window, VkInstance instance)
	{
		VkSurfaceKHR surface;

		if (SDL_Vulkan_CreateSurface(window, instance, &surface) == false) {
			throw std::runtime_error("Unable to create window surface");
		}

		return surface;
	}

	// returns the queue supporting the requested flags
	static Queue getQueueSupporting(const std::vector<Queue> queues, QueueFlags flags)
	{
		uint32_t bitmask = static_cast<uint32_t>(flags);

		for (size_t i = 0; i < queues.size(); i++) {

			if (bitmask & static_cast<uint32_t>(QueueFlags::GRAPHICS)) {
				if (queues[i].supportsGraphics == false) continue;
			}

			if (bitmask & static_cast<uint32_t>(QueueFlags::TRANSFER)) {
				if (queues[i].supportsTransfer == false) continue;
			}

			if (bitmask & static_cast<uint32_t>(QueueFlags::COMPUTE)) {
				if (queues[i].supportsCompute == false) continue;
			}

			return queues[i];

		}

		throw std::runtime_error("Unable to find the requested queue");
	}

	static Swapchain::MSTarget createMSAATarget(VkSampleCountFlagBits msaaSamples, VkExtent2D extent, VkFormat colorFormat, VkDevice device, VmaAllocator allocator)
	{
		Swapchain::MSTarget target{};

		[[maybe_unused]] VkResult res;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = extent.width;
		imageInfo.extent.height = extent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = colorFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = msaaSamples;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocInfo.priority = 1.0f;

		res = vmaCreateImage(allocator, &imageInfo, &allocInfo, &target.colorImage, &target.colorImageAllocation, nullptr);
		assert(res == VK_SUCCESS);

		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = target.colorImage;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = colorFormat;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		res = vkCreateImageView(device, &imageViewInfo, nullptr, &target.colorImageView);
		assert(res == VK_SUCCESS);

		return target;
	}

	static void destroyMSAATarget(const Swapchain::MSTarget& target, VkDevice device, VmaAllocator allocator)
	{
		vkDestroyImageView(device, target.colorImageView, nullptr);
		vmaDestroyImage(allocator, target.colorImage, target.colorImageAllocation);
	}

	static DepthBuffer createDepthBuffer(VkDevice device, VmaAllocator allocator, VkExtent2D extent, VkSampleCountFlagBits msaaSamples)
	{
		DepthBuffer db{};

		[[maybe_unused]] VkResult res;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = extent.width;
		imageInfo.extent.height = extent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_D32_SFLOAT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = msaaSamples;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocInfo.priority = 1.0f;

		res = vmaCreateImage(allocator, &imageInfo, &allocInfo, &db.image, &db.allocation, nullptr);
		assert(res == VK_SUCCESS);

		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = db.image;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_D32_SFLOAT;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		res = vkCreateImageView(device, &imageViewInfo, nullptr, &db.view);
		assert(res == VK_SUCCESS);

		return db;
	}

	static void destroyDepthBuffer(DepthBuffer db, VkDevice device, VmaAllocator allocator)
	{
		vkDestroyImageView(device, db.view, nullptr);
		vmaDestroyImage(allocator, db.image, db.allocation);
	}

	static VkSampleCountFlagBits getMaxSampleCount(VkPhysicalDevice physicalDevice)
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		counts %= VK_SAMPLE_COUNT_4_BIT; // restricts it to 2 or 1 (0b11)
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
		throw std::runtime_error("MSAA is not supported");
	}

	// This is called not just on initialisation, but also when the window is resized.
	static void createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator allocator, std::vector<Queue> queues, SDL_Window* window, VkSurfaceKHR surface, bool vsync, Swapchain* swapchain)
	{
		[[maybe_unused]] VkResult res;

		// get surface capabilities
		VkSurfaceCapabilitiesKHR caps;
		res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
		assert(res == VK_SUCCESS);

		// check there is at least one supported surface format
		uint32_t surfaceFormatCount = 0;
		std::vector<VkSurfaceFormatKHR> formats{};
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
		assert(res == VK_SUCCESS);
		formats.resize(surfaceFormatCount);
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, formats.data());
		assert(res == VK_SUCCESS);

		// check there is at least one supported present mode
		uint32_t surfacePresentModeCount = 0;
		std::vector<VkPresentModeKHR> presentModes{};
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr);
		assert(res == VK_SUCCESS);
		presentModes.resize(surfacePresentModeCount);
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, presentModes.data());
		assert(res == VK_SUCCESS);

		VkExtent2D oldExtent = swapchain->extent;

		if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			swapchain->extent = caps.currentExtent;
		}
		else {
			// if fb size isn't already found, get it from SDL
			int width, height;
			SDL_Vulkan_GetDrawableSize(window, &width, &height);

			swapchain->extent.width = static_cast<uint32_t>(width);
			swapchain->extent.height = static_cast<uint32_t>(height);

			swapchain->extent.width = std::clamp(
				swapchain->extent.width,
				caps.minImageExtent.width, caps.maxImageExtent.width);
			swapchain->extent.height = std::clamp(
				swapchain->extent.height,
				caps.minImageExtent.height, caps.maxImageExtent.height);
		}

		if (swapchain->extent.width == 0 || swapchain->extent.height == 0) {
			swapchain->extent = oldExtent;
		}

		// delete old framebuffers
		for (VkFramebuffer fb : swapchain->framebuffers) {
			vkDestroyFramebuffer(device, fb, nullptr);
		}

		// delete old image views
		for (VkImageView view : swapchain->imageViews) {
			vkDestroyImageView(device, view, nullptr);
		}

		swapchain->surfaceFormat = formats[0];
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				swapchain->surfaceFormat = format; // prefer using srgb non linear colors
			}
		}

		swapchain->presentMode = VK_PRESENT_MODE_FIFO_KHR; // This mode is always available
		if (vsync == false) {
			for (const auto& presMode : presentModes) {
				if (presMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					swapchain->presentMode = presMode; // this mode allows uncapped FPS while also avoiding screen tearing
				}
			}
		}

		uint32_t imageCount = caps.minImageCount + 1;
		if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
			imageCount = caps.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = nullptr,
			.flags = 0,
			.surface = surface,
			.minImageCount = imageCount,
			.imageFormat = swapchain->surfaceFormat.format,
			.imageColorSpace = swapchain->surfaceFormat.colorSpace,
			.imageExtent = swapchain->extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.preTransform = caps.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = swapchain->presentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = swapchain->swapchain,

		};

		std::array<uint32_t, 2> queueFamilyIndices{
			getQueueSupporting(queues, QueueFlags::GRAPHICS).familyIndex,
			getQueueSupporting(queues, QueueFlags::TRANSFER).familyIndex
		};

		// if graphics and transfer queues aren't in the same family
		if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
			throw std::runtime_error("Graphics and transfer queues must be in the same family");
		}

		res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain->swapchain);
		assert(res == VK_SUCCESS);

		if (createInfo.oldSwapchain != VK_NULL_HANDLE) {
			// if recreating swapchain, destroy old one
			vkDestroySwapchainKHR(device, createInfo.oldSwapchain, nullptr);
		}

		// get all the image handles
		uint32_t swapchainImageCount = 0;
		res = vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchainImageCount, nullptr);
		assert(res == VK_SUCCESS);
		swapchain->images.resize(swapchainImageCount);
		res = vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchainImageCount, swapchain->images.data());
		assert(res == VK_SUCCESS);

		// Use multisample anti-aliasing
		swapchain->msaaSamples = getMaxSampleCount(physicalDevice);

		// create depth buffer if old depth buffer is wrong size.
		// Also do the same for the MSAA buffer.
		if (swapchain->swapchain == VK_NULL_HANDLE) {
			swapchain->depthBuffer = createDepthBuffer(device, allocator, swapchain->extent, swapchain->msaaSamples);
			swapchain->msTarget = createMSAATarget(swapchain->msaaSamples, swapchain->extent, swapchain->surfaceFormat.format, device, allocator);
		}
		else if (swapchain->extent.width != oldExtent.width || swapchain->extent.height != oldExtent.height) {
			destroyDepthBuffer(swapchain->depthBuffer, device, allocator);
			swapchain->depthBuffer = createDepthBuffer(device, allocator, swapchain->extent, swapchain->msaaSamples);
			destroyMSAATarget(swapchain->msTarget, device, allocator);
			swapchain->msTarget = createMSAATarget(swapchain->msaaSamples, swapchain->extent, swapchain->surfaceFormat.format, device, allocator);
		}

		VkAttachmentReference colorAttachmentRef{};
		VkAttachmentReference depthAttachmentRef{};
		VkAttachmentReference colorAttachmentResolveRef{};

		// create the render pass
		if (swapchain->renderpass == VK_NULL_HANDLE) {

			/*
			 * render pass layout:
			 * 0: color attachment with msaa samples, 
			 * 1: depth attachment with msaa samples, used for fragment shading
			 * 2: present src (resolve) attachment with 1 sample, used for swapchain present
			 */

			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = swapchain->surfaceFormat.format;
			colorAttachment.samples = swapchain->msaaSamples;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = VK_FORMAT_D32_SFLOAT;
			depthAttachment.samples = swapchain->msaaSamples;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription colorAttachmentResolve{};
			colorAttachmentResolve.format = swapchain->surfaceFormat.format;
			colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			colorAttachmentResolveRef.attachment = 2;
			colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;
			subpass.pResolveAttachments = &colorAttachmentResolveRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

			VkRenderPassCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfo.attachmentCount = (uint32_t)attachments.size();
			createInfo.pAttachments = attachments.data();
			createInfo.subpassCount = 1;
			createInfo.pSubpasses = &subpass;
			createInfo.dependencyCount = 1;
			createInfo.pDependencies = &dependency;

			res = vkCreateRenderPass(device, &createInfo, nullptr, &swapchain->renderpass);
		}

		// create image views and framebuffers

		swapchain->imageViews.resize(swapchain->images.size());
		swapchain->framebuffers.resize(swapchain->images.size());
		for (size_t i = 0; i < swapchain->images.size(); i++) {

			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.image = swapchain->images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapchain->surfaceFormat.format;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			res = vkCreateImageView(device, &createInfo, nullptr, &swapchain->imageViews[i]);
			assert(res == VK_SUCCESS);

			std::array<VkImageView, 3> attachments = {
				swapchain->msTarget.colorImageView,
				swapchain->depthBuffer.view,
				swapchain->imageViews[i],
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = swapchain->renderpass;
			framebufferInfo.attachmentCount = (uint32_t)attachments.size();
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapchain->extent.width;
			framebufferInfo.height = swapchain->extent.height;
			framebufferInfo.layers = 1;

			res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain->framebuffers[i]);
			assert(res == VK_SUCCESS);

		}

		// create the swapchain semaphores
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		for (auto& acquireSemaphore : swapchain->acquireSemaphores) {
			if (acquireSemaphore == VK_NULL_HANDLE) {
				res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &acquireSemaphore);
				assert(res == VK_SUCCESS);
			}
		}
		for (auto& releaseSemaphore : swapchain->releaseSemaphores) {
			if (releaseSemaphore == VK_NULL_HANDLE) {
				res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &releaseSemaphore);
				assert(res == VK_SUCCESS);
			}
		}

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
		assert(res == VK_SUCCESS);

		{ // record the command buffer
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			assert(res == VK_SUCCESS);

			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = size;
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

			res = vkEndCommandBuffer(commandBuffer);
			assert(res == VK_SUCCESS);
		}

		// submit
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(res == VK_SUCCESS);

		res = vkQueueWaitIdle(queue);
		assert(res == VK_SUCCESS);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		
	}

	VkCommandBuffer beginOneTimeCommands(VkDevice device, VkCommandPool commandPool)
	{
		[[maybe_unused]] VkResult res;

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
		assert(res == VK_SUCCESS);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		assert(res == VK_SUCCESS);

		return commandBuffer;
	}

	static void endOneTimeCommands(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue)
	{
		[[maybe_unused]] VkResult res;
		res = vkEndCommandBuffer(commandBuffer);
		assert(res == VK_SUCCESS);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(res == VK_SUCCESS);
		res = vkQueueWaitIdle(queue);
		assert(res == VK_SUCCESS);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	static void cmdTransitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, VkImage image)
	{

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	}

	static void cmdGenerateMipmaps(VkCommandBuffer commandBuffer, VkImage image, int32_t width, int32_t height, uint32_t mipLevels)
	{

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = width;
		int32_t mipHeight = height;
		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;

		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	// class definitions

	struct GFXDevice::Impl {
		
		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

		SDL_Window* window = nullptr;
		
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		
		Swapchain swapchain{};

		std::vector<Queue> queues{};
		Queue gfxQueue{};
		Queue presentQueue{};
		VkCommandPool commandPool = VK_NULL_HANDLE;

		VmaAllocator allocator = nullptr;

		// device settings
		bool vsync = false;
		float maxSamplerAnisotropy;

		// render loop
		uint64_t FRAMECOUNT = 0;
		std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers{};
		std::array<VkFence, FRAMES_IN_FLIGHT> inFlightFences{};
		std::queue<DrawCall> drawQueue{};
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		VkDescriptorSetLayout descriptorSetLayout{};
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		VkDescriptorSetLayout samplerSetLayout{};

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window, bool vsync)
	{

		pimpl = std::make_unique<Impl>();

		VkResult res;

		pimpl->window = window;
		pimpl->vsync = vsync;

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

		bool useValidation;
#ifdef NDEBUG
		useValidation = false; // release mode
#else
		useValidation = true; // debug mode
#endif

		// get the both the engine and application versions
		int appVersionMajor = 0, appVersionMinor = 0, appVersionPatch = 0;
		versionFromCharArray(appVersion, &appVersionMajor, &appVersionMinor, &appVersionPatch);
		int engineVersionMajor = 0, engineVersionMinor = 0, engineVersionPatch = 0;
		versionFromCharArray(ENGINE_VERSION, &engineVersionMajor, &engineVersionMinor, &engineVersionPatch);

		VkApplicationInfo applicationInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = appName,
			.applicationVersion = VK_MAKE_VERSION(appVersionMajor, appVersionMinor, appVersionPatch),
			.pEngineName = "engine",
			.engineVersion = VK_MAKE_VERSION(engineVersionMajor, engineVersionMinor, engineVersionPatch),
			.apiVersion = VK_API_VERSION_1_3,
		};



		// make a list of all extensions to use
		std::vector<const char*> extensions{};

		const std::vector<const char*> windowExtensions = getRequiredVulkanExtensions(window);
		extensions.insert(extensions.end(), windowExtensions.begin(), windowExtensions.end());

		// also use debug utils extension
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);



		// make a list of layers to use
		std::vector<const char*> layers{};

		const LayerInfo layerInfo = getAvailableLayers(useValidation);

		if (layerInfo.validationLayer.has_value()) {
			layers.push_back(layerInfo.validationLayer.value()->layerName);
		}

		VkInstanceCreateInfo instanceInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pApplicationInfo = &applicationInfo,
			.enabledLayerCount = (uint32_t)layers.size(),
			.ppEnabledLayerNames = layers.data(),
			.enabledExtensionCount = (uint32_t)extensions.size(),
			.ppEnabledExtensionNames = extensions.data(),
		};

		// add the debug messenger
		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo;
		if (layerInfo.validationLayer.has_value()) {
			debugMessengerInfo = getDebugMessengerCreateInfo();
			instanceInfo.pNext = &debugMessengerInfo;
		}
		else {
			instanceInfo.pNext = nullptr;
		}



		for ([[maybe_unused]] const char* ext : extensions) {
			DEBUG("Using Vulkan instance extension: {}", ext);
		}

		res = vkCreateInstance(&instanceInfo, nullptr, &pimpl->instance);
		if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
			throw std::runtime_error("The graphics driver is incompatible with vulkan");
		} else if (res != VK_SUCCESS) {
			throw std::runtime_error("vkCreateInstance failed: " + std::to_string(res));
		}



		// load the instance functions
		volkLoadInstanceOnly(pimpl->instance);



		// create the debug messenger
		{
			VkDebugUtilsMessengerCreateInfoEXT createInfo = getDebugMessengerCreateInfo();

			VkResult res;
			res = vkCreateDebugUtilsMessengerEXT(pimpl->instance, &createInfo, nullptr, &pimpl->debugMessenger);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("vkCreateDebugUtilsMessengerExt failed: " + std::to_string(res));
			}
		}



		// get the surface
		pimpl->surface = createSurface(window, pimpl->instance);



		// Select a physical device and get capabilities, features, and display modes.
		// Create a logical device and create the queues and their corresponding command buffers.
		{
			// enumerate physical devices
			uint32_t physDeviceCount = 0;
			VkResult res;
			res = vkEnumeratePhysicalDevices(pimpl->instance, &physDeviceCount, nullptr);
			assert(res == VK_SUCCESS);
			if (physDeviceCount == 0) {
				throw std::runtime_error("No GPU found with vulkan support!");
			}
			std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
			res = vkEnumeratePhysicalDevices(pimpl->instance, &physDeviceCount, physicalDevices.data());
			assert(res == VK_SUCCESS);

			// find suitable device:

			const std::vector<const char*> requiredDeviceExtensions{
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};

			for (const auto& dev : physicalDevices) {

				// first, check extension support
				uint32_t extensionCount;
				res = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
				assert(res == VK_SUCCESS);
				std::vector<VkExtensionProperties> availableExtensions(extensionCount);
				res = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, availableExtensions.data());
				assert(res == VK_SUCCESS);

				for (const char* extToFind : requiredDeviceExtensions) {
					bool extFound = false;
					for (const auto& ext : availableExtensions) {
						if (strcmp(extToFind, ext.extensionName) == 0) {
							extFound = true;
						}
					}
					if (!extFound) {
						continue;
					}
				}



				// check physical device properties
				VkPhysicalDeviceProperties devProps;
				vkGetPhysicalDeviceProperties(dev, &devProps);

				// check that the device supports vulkan 1.3
				if (devProps.apiVersion < VK_API_VERSION_1_3) {
					continue;
				}

				// check for some features:
				VkPhysicalDeviceFeatures devFeatures;
				vkGetPhysicalDeviceFeatures(dev, &devFeatures);
				// anisotropic filtering is needed
				if (devFeatures.samplerAnisotropy == VK_FALSE) continue;
				pimpl->maxSamplerAnisotropy = devProps.limits.maxSamplerAnisotropy;

				// check for linear filtering for mipmaps
				VkFormatProperties formatProperties{};
				vkGetPhysicalDeviceFormatProperties(dev, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
				if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
					continue;
				}

				pimpl->physicalDevice = dev;
				break;

			} // end for()

			if (pimpl->physicalDevice == VK_NULL_HANDLE) {
				throw std::runtime_error("No suitable Vulkan physical device found");
			}

			VkPhysicalDeviceProperties devProps;
			vkGetPhysicalDeviceProperties(pimpl->physicalDevice, &devProps);
			DEBUG("Selected physical device: {}", devProps.deviceName);



			// Get the queue families and find ones that support graphics, transfer, and compute

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(pimpl->physicalDevice, &queueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(pimpl->physicalDevice, &queueFamilyCount, queueFamilies.data());

			std::optional<uint32_t> graphicsFamilyIndex;
			std::optional<uint32_t> transferFamilyIndex;
			std::optional<uint32_t> computeFamilyIndex;

			for (uint32_t i = 0; i < queueFamilyCount; i++) {
				VkQueueFamilyProperties family = queueFamilies[i];
				if (family.queueCount > 0) {
					if (graphicsFamilyIndex.has_value() == false && family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
						graphicsFamilyIndex = i;
					}
					if (transferFamilyIndex.has_value() == false && family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
						transferFamilyIndex = i;
					}
					if (computeFamilyIndex.has_value() == false && family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
						computeFamilyIndex = i;
					}
				}
			}
			if (graphicsFamilyIndex.has_value() == false ||
				transferFamilyIndex.has_value() == false) {
				throw std::runtime_error("Unable to find queues with the GRAPHICS or TRANSFER family flags");
			}

			// there is no guaranteed support for compute queues

			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

			// use a set to filter out duplicate indices
			std::unordered_set<uint32_t> uniqueQueueFamilies{};
			if (graphicsFamilyIndex.has_value()) uniqueQueueFamilies.insert(graphicsFamilyIndex.value());
			if (transferFamilyIndex.has_value()) uniqueQueueFamilies.insert(transferFamilyIndex.value());
			if (computeFamilyIndex.has_value()) uniqueQueueFamilies.insert(computeFamilyIndex.value());

			float queuePriority = 1.0f;

			for (uint32_t family : uniqueQueueFamilies) {
				// create a queue for each unique type to ensure that there are queues available for graphics, transfer, and compute

				Queue newQueue{};
				newQueue.familyIndex = family;
				newQueue.queueIndex = 0;
				newQueue.supportsGraphics = false;
				newQueue.supportsTransfer = false;
				newQueue.supportsCompute = false;
				if (graphicsFamilyIndex == family) newQueue.supportsGraphics = true;
				if (transferFamilyIndex == family) newQueue.supportsTransfer = true;
				if (computeFamilyIndex == family) newQueue.supportsCompute = true;

				VkDeviceQueueCreateInfo queueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.queueFamilyIndex = family,
					.queueCount = 1,
					.pQueuePriorities = &queuePriority,
				};
				queueCreateInfos.push_back(queueCreateInfo);
				pimpl->queues.push_back(newQueue);
			}

			// check the physical device is compatible with the surface
			VkBool32 graphicsQueueCanPresent;
			res = vkGetPhysicalDeviceSurfaceSupportKHR(pimpl->physicalDevice, graphicsFamilyIndex.value(), pimpl->surface, &graphicsQueueCanPresent);
			assert(res == VK_SUCCESS);
			if (graphicsQueueCanPresent != VK_TRUE) {
				throw std::runtime_error("The selected queue family does not support this surface");
			}

			VkPhysicalDeviceFeatures deviceFeatures{};
			deviceFeatures.samplerAnisotropy = VK_TRUE;

			VkDeviceCreateInfo deviceCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.queueCreateInfoCount = (uint32_t)queueCreateInfos.size(),
				.pQueueCreateInfos = queueCreateInfos.data(),
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,
				.enabledExtensionCount = (uint32_t)requiredDeviceExtensions.size(),
				.ppEnabledExtensionNames = requiredDeviceExtensions.data(),
				.pEnabledFeatures = &deviceFeatures,
			};

			res = vkCreateDevice(pimpl->physicalDevice, &deviceCreateInfo, nullptr, &pimpl->device);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("Unable to create Vulkan logical device, error code: " + std::to_string(res));
			}

			volkLoadDevice(pimpl->device);

			for (auto& q : pimpl->queues) {
				vkGetDeviceQueue(pimpl->device, q.familyIndex, q.queueIndex, &q.handle);
			}

			pimpl->presentQueue = getQueueSupporting(pimpl->queues, QueueFlags::TRANSFER);
			pimpl->gfxQueue = getQueueSupporting(pimpl->queues, QueueFlags::GRAPHICS);

			VkCommandPoolCreateInfo gfxCmdPoolInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = pimpl->gfxQueue.familyIndex
			};

			res = vkCreateCommandPool(pimpl->device, &gfxCmdPoolInfo, nullptr, &pimpl->commandPool);
			assert(res == VK_SUCCESS);

			VkCommandBufferAllocateInfo gfxCmdBufInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = pimpl->commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
				res = vkAllocateCommandBuffers(pimpl->device, &gfxCmdBufInfo, &pimpl->commandBuffers[i]);
				assert(res == VK_SUCCESS);
			}
		}



		// now make the memory allocator using vk_mem_alloc.h
		{
			VmaVulkanFunctions functions{
				.vkGetInstanceProcAddr = nullptr,
				.vkGetDeviceProcAddr = nullptr,
				.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
				.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
				.vkAllocateMemory = vkAllocateMemory,
				.vkFreeMemory = vkFreeMemory,
				.vkMapMemory = vkMapMemory,
				.vkUnmapMemory = vkUnmapMemory,
				.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
				.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
				.vkBindBufferMemory = vkBindBufferMemory,
				.vkBindImageMemory = vkBindImageMemory,
				.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
				.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
				.vkCreateBuffer = vkCreateBuffer,
				.vkDestroyBuffer = vkDestroyBuffer,
				.vkCreateImage = vkCreateImage,
				.vkDestroyImage = vkDestroyImage,
				.vkCmdCopyBuffer = vkCmdCopyBuffer,
				.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
				.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
				.vkBindBufferMemory2KHR = vkBindBufferMemory2,
				.vkBindImageMemory2KHR = vkBindImageMemory2,
				.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
				.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
				.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
			};

			VmaAllocatorCreateInfo createInfo{
				.flags = 0,
				.physicalDevice = pimpl->physicalDevice,
				.device = pimpl->device,
				.preferredLargeHeapBlockSize = 0,
				.pAllocationCallbacks = nullptr,
				.pDeviceMemoryCallbacks = nullptr,
				.pHeapSizeLimit = nullptr,
				.pVulkanFunctions = &functions,
				.instance = pimpl->instance,
				.vulkanApiVersion = VK_API_VERSION_1_3,
				.pTypeExternalMemoryHandleTypes = nullptr
			};

			[[maybe_unused]] VkResult res;
			res = vmaCreateAllocator(&createInfo, &pimpl->allocator);
			assert(res == VK_SUCCESS);
		}



		// Now make the swapchain
		createSwapchain(pimpl->device, pimpl->physicalDevice, pimpl->allocator, pimpl->queues, window, pimpl->surface, pimpl->vsync, &pimpl->swapchain);



		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		fenceInfo.pNext = nullptr;
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			res = vkCreateFence(pimpl->device, &fenceInfo, nullptr, &pimpl->inFlightFences[i]);
			assert(res == VK_SUCCESS);
		}

		// create uniform buffer descriptor set layout
		pimpl->uboLayoutBinding.binding = 0;
		pimpl->uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pimpl->uboLayoutBinding.descriptorCount = 1;
		pimpl->uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		pimpl->uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = 1;
		descriptorSetLayoutInfo.pBindings = &pimpl->uboLayoutBinding;
		res = vkCreateDescriptorSetLayout(pimpl->device, &descriptorSetLayoutInfo, nullptr, &pimpl->descriptorSetLayout);
		assert(res == VK_SUCCESS);

		// create texture sampler descriptor set layout
		pimpl->samplerLayoutBinding.binding = 0;
		pimpl->samplerLayoutBinding.descriptorCount = 1;
		pimpl->samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pimpl->samplerLayoutBinding.pImmutableSamplers = nullptr;
		pimpl->samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo samplerSetLayoutInfo{};
		samplerSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		samplerSetLayoutInfo.bindingCount = 1;
		samplerSetLayoutInfo.pBindings = &pimpl->samplerLayoutBinding;
		res = vkCreateDescriptorSetLayout(pimpl->device, &samplerSetLayoutInfo, nullptr, &pimpl->samplerSetLayout);
		assert(res == VK_SUCCESS);


	}

	GFXDevice::~GFXDevice()
	{
		vkDestroyDescriptorSetLayout(pimpl->device, pimpl->samplerSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(pimpl->device, pimpl->descriptorSetLayout, nullptr);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			vkDestroyFence(pimpl->device, pimpl->inFlightFences[i], nullptr);
			vkDestroySemaphore(pimpl->device, pimpl->swapchain.releaseSemaphores[i], nullptr);
			vkDestroySemaphore(pimpl->device, pimpl->swapchain.acquireSemaphores[i], nullptr);
		}
		for (VkImageView view : pimpl->swapchain.imageViews) {
			vkDestroyImageView(pimpl->device, view, nullptr);
		}
		for (VkFramebuffer fb : pimpl->swapchain.framebuffers) {
			vkDestroyFramebuffer(pimpl->device, fb, nullptr);
		}
		destroyMSAATarget(pimpl->swapchain.msTarget, pimpl->device, pimpl->allocator);
		destroyDepthBuffer(pimpl->swapchain.depthBuffer, pimpl->device, pimpl->allocator);
		vkDestroyRenderPass(pimpl->device, pimpl->swapchain.renderpass, nullptr);
		vkDestroySwapchainKHR(pimpl->device, pimpl->swapchain.swapchain, nullptr);

		vmaDestroyAllocator(pimpl->allocator);

		vkDestroyCommandPool(pimpl->device, pimpl->commandPool, nullptr);
		vkDestroyDevice(pimpl->device, nullptr);
		vkDestroySurfaceKHR(pimpl->instance, pimpl->surface, nullptr);
		vkDestroyDebugUtilsMessengerEXT(pimpl->instance, pimpl->debugMessenger, nullptr);
		vkDestroyInstance(pimpl->instance, nullptr);
	}

	void GFXDevice::getViewportSize(uint32_t *w, uint32_t *h)
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

	void GFXDevice::draw(const gfx::Pipeline* pipeline, const gfx::Buffer* vertexBuffer, const gfx::Buffer* indexBuffer, uint32_t count, const void* pushConstantData, size_t pushConstantSize, const gfx::Texture* texture)
	{
		assert(pipeline != nullptr);
		assert(vertexBuffer != nullptr);
		assert(vertexBuffer->type == gfx::BufferType::VERTEX);
		assert(indexBuffer == nullptr || indexBuffer->type == gfx::BufferType::INDEX);
		assert(pushConstantSize <= PUSH_CONSTANT_MAX_SIZE);

		DrawCall call{
			.pipeline = pipeline,
			.vertexBuffer = vertexBuffer,
			.indexBuffer = indexBuffer, // will be ignored if nullptr
			.count = count,
			.pushConstantData{}
		};

		memcpy(call.pushConstantData, pushConstantData, pushConstantSize);

		call.texture = texture; // will be ignored if nullptr

		pimpl->drawQueue.push(call);

	}

	void GFXDevice::renderFrame()
	{
		VkResult res;

		const uint32_t frameIndex = pimpl->FRAMECOUNT % FRAMES_IN_FLIGHT;

		res = vkWaitForFences(pimpl->device, 1, &pimpl->inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
		assert(res == VK_SUCCESS);
		res = vkResetFences(pimpl->device, 1, &pimpl->inFlightFences[frameIndex]);
		assert(res == VK_SUCCESS);

		uint32_t imageIndex = 0;
		res = vkAcquireNextImageKHR(pimpl->device, pimpl->swapchain.swapchain, UINT64_MAX, pimpl->swapchain.acquireSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);
		if (res == VK_ERROR_OUT_OF_DATE_KHR) {
			// recreate swapchain
			waitIdle();
			createSwapchain(pimpl->device, pimpl->physicalDevice, pimpl->allocator, pimpl->queues, pimpl->window, pimpl->surface, pimpl->vsync, &pimpl->swapchain);
			return;
		}
		else {
			assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
		}

		res = vkResetCommandBuffer(pimpl->commandBuffers[frameIndex], 0);
		assert(res == VK_SUCCESS);
		
		// now record command buffer
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;
			res = vkBeginCommandBuffer(pimpl->commandBuffers[frameIndex], &beginInfo);
			assert(res == VK_SUCCESS);

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = pimpl->swapchain.renderpass;
			renderPassInfo.framebuffer = pimpl->swapchain.framebuffers[imageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = pimpl->swapchain.extent;

			std::array<VkClearValue, 2> clearValues{};
			clearValues[0].color = { {1.0f, 1.0f, 1.0f, 1.0f} };
			clearValues[1].depthStencil = { 1.0f, 0 };
			renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
			renderPassInfo.pClearValues = clearValues.data();
			
			vkCmdBeginRenderPass(pimpl->commandBuffers[frameIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = (float)pimpl->swapchain.extent.height;
			viewport.width = (float)pimpl->swapchain.extent.width;
			viewport.height = -(float)pimpl->swapchain.extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(pimpl->commandBuffers[frameIndex], 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = pimpl->swapchain.extent;
			vkCmdSetScissor(pimpl->commandBuffers[frameIndex], 0, 1, &scissor);

			// run queued draw calls

			VkDeviceSize offsets[] = { 0 };

			const gfx::Pipeline* lastPipeline = nullptr;
			const gfx::Texture* lastTexture = nullptr;
			const gfx::Buffer* lastVertexBuffer = nullptr;
			const gfx::Buffer* lastIndexBuffer = nullptr;
			while (pimpl->drawQueue.empty() == false) {
				
				DrawCall call = pimpl->drawQueue.front();

				if (call.pipeline != lastPipeline) {
					vkCmdBindPipeline(pimpl->commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, call.pipeline->handle);
					// bind pipeline uniform-buffer
					vkCmdBindDescriptorSets(pimpl->commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, call.pipeline->layout, 0, 1, &call.pipeline->descriptorSets[frameIndex], 0, nullptr);
				}
					
				if (call.texture != lastTexture) {
					// set the texture
					vkCmdBindDescriptorSets(pimpl->commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, call.pipeline->layout, 1, 1, &call.texture->descriptorSets[frameIndex], 0, nullptr);
				}
					
				// like uniforms but faster
				vkCmdPushConstants(pimpl->commandBuffers[frameIndex], call.pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, PUSH_CONSTANT_MAX_SIZE, call.pushConstantData);

				if (call.vertexBuffer != lastVertexBuffer) {
					vkCmdBindVertexBuffers(pimpl->commandBuffers[frameIndex], 0, 1, &call.vertexBuffer->buffer, offsets);
				}
				if (call.indexBuffer == nullptr) {
					// no index buffer
					vkCmdDraw(pimpl->commandBuffers[frameIndex], call.count, 1, 0, 0);
				} else {
					// use index buffer
					if (call.indexBuffer != lastIndexBuffer) {
						vkCmdBindIndexBuffer(pimpl->commandBuffers[frameIndex], call.indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
					}
					vkCmdDrawIndexed(pimpl->commandBuffers[frameIndex], call.count, 1, 0, 0, 0);
				}

				lastPipeline = call.pipeline;
				lastTexture = call.texture;
				lastVertexBuffer = call.vertexBuffer;
				lastIndexBuffer = call.indexBuffer;

				pimpl->drawQueue.pop();
				
			}

			vkCmdEndRenderPass(pimpl->commandBuffers[frameIndex]);

			res = vkEndCommandBuffer(pimpl->commandBuffers[frameIndex]);
			assert(res == VK_SUCCESS);
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &pimpl->swapchain.acquireSemaphores[frameIndex];
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &pimpl->commandBuffers[frameIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &pimpl->swapchain.releaseSemaphores[frameIndex];

		res = vkQueueSubmit(pimpl->gfxQueue.handle, 1, &submitInfo, pimpl->inFlightFences[frameIndex]);
		assert(res == VK_SUCCESS);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &pimpl->swapchain.releaseSemaphores[frameIndex];

		VkSwapchainKHR swapchains[] = { pimpl->swapchain.swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		res = vkQueuePresentKHR(pimpl->presentQueue.handle, &presentInfo);
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
			// recreate swapchain
			waitIdle();
			createSwapchain(pimpl->device, pimpl->physicalDevice, pimpl->allocator, pimpl->queues, pimpl->window, pimpl->surface, pimpl->vsync, &pimpl->swapchain);
		}
		else {
			assert(res == VK_SUCCESS);
		}

		pimpl->FRAMECOUNT++;
	}
	
	gfx::Pipeline* GFXDevice::createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat, uint64_t uniformBufferSize, bool alphaBlending, bool backfaceCulling)
	{

		[[maybe_unused]] VkResult res;

		gfx::Pipeline* pipeline = new gfx::Pipeline;

		auto vertShaderCode = util::readTextFile(vertShaderPath);
		auto fragShaderCode = util::readTextFile(fragShaderPath);

		VkShaderModule vertShaderModule = compileShader(pimpl->device, shaderc_vertex_shader, vertShaderCode->data(), vertShaderPath);
		VkShaderModule fragShaderModule = compileShader(pimpl->device, shaderc_fragment_shader, fragShaderCode->data(), fragShaderPath);
		
		// create uniform buffers
		pipeline->uniformBuffers.resize(FRAMES_IN_FLIGHT);
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			auto buf = new gfx::Buffer{};
			buf->size = uniformBufferSize;
			buf->type = gfx::BufferType::UNIFORM;

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = buf->size;
			bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo allocInfo{};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			VmaAllocationInfo resultingAlloc;
			res = vmaCreateBuffer(pimpl->allocator, &bufferInfo, &allocInfo, &buf->buffer, &buf->allocation, &resultingAlloc);
			assert(res == VK_SUCCESS);

			pipeline->uniformBuffers[i] = buf;
		}

		// create descriptor pools
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = FRAMES_IN_FLIGHT;
		
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = FRAMES_IN_FLIGHT;
		res = vkCreateDescriptorPool(pimpl->device, &poolInfo, nullptr, &pipeline->descriptorPool);
		assert(res == VK_SUCCESS);

		std::array<VkDescriptorSetLayout, FRAMES_IN_FLIGHT> layouts;
		layouts.fill(pimpl->descriptorSetLayout);
		VkDescriptorSetAllocateInfo dSetAllocInfo{};
		dSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dSetAllocInfo.descriptorPool = pipeline->descriptorPool;
		dSetAllocInfo.descriptorSetCount = FRAMES_IN_FLIGHT;
		dSetAllocInfo.pSetLayouts = layouts.data();
		res = vkAllocateDescriptorSets(pimpl->device, &dSetAllocInfo, pipeline->descriptorSets.data());
		assert(res == VK_SUCCESS);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = pipeline->uniformBuffers[i]->buffer;
			bufferInfo.offset = 0;
			bufferInfo.range = uniformBufferSize;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = pipeline->descriptorSets[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = &bufferInfo;
			descriptorWrite.pImageInfo = nullptr;
			descriptorWrite.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(pimpl->device, 1, &descriptorWrite, 0, nullptr);
		}

		// get vertex attrib layout:
		VkVertexInputBindingDescription bindingDescription{ };
		bindingDescription.binding = 0;
		bindingDescription.stride = vertexFormat.stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attribDescs{};
		attribDescs.reserve(vertexFormat.attributeDescriptions.size());
		for (const auto& desc : vertexFormat.attributeDescriptions) {
			VkVertexInputAttributeDescription vulkanAttribDesc{};
			vulkanAttribDesc.binding = 0;
			vulkanAttribDesc.location = desc.location;
			vulkanAttribDesc.offset = desc.offset;
			vulkanAttribDesc.format = vkinternal::getVertexAttribFormat(desc.format);
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

		VkPipelineShaderStageCreateInfo shaderStages[2] = { vertShaderStageInfo, fragShaderStageInfo };

		// this sets "vertex attribute pointers"
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attribDescs.size();
		vertexInputInfo.pVertexAttributeDescriptions = attribDescs.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = (float)pimpl->swapchain.extent.height;
		viewport.width = (float)pimpl->swapchain.extent.width;
		viewport.height = -(float)pimpl->swapchain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = pimpl->swapchain.extent;

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		if (backfaceCulling == true) {
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		}
		else {
			rasterizer.cullMode = VK_CULL_MODE_NONE;
		}
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // ignored
		rasterizer.depthBiasClamp = 0.0f; // ignored
		rasterizer.depthBiasSlopeFactor = 0.0f; // ignored

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = pimpl->swapchain.msaaSamples;
		multisampling.minSampleShading = 1.0f; // ignored
		multisampling.pSampleMask = nullptr; // ignored
		multisampling.alphaToCoverageEnable = VK_FALSE; // ignored
		multisampling.alphaToOneEnable = VK_FALSE; // ignored

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		if (alphaBlending) {
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
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
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

		std::array<VkDescriptorSetLayout, 2> setLayouts{ pimpl->descriptorSetLayout, pimpl->samplerSetLayout};

		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
		layoutInfo.pSetLayouts = setLayouts.data();
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstantRange;

		res = vkCreatePipelineLayout(pimpl->device, &layoutInfo, nullptr, &pipeline->layout);
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
		createInfo.renderPass = pimpl->swapchain.renderpass;
		createInfo.subpass = 0;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		res = vkCreateGraphicsPipelines(pimpl->device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->handle);
		assert(res == VK_SUCCESS);

		vkDestroyShaderModule(pimpl->device, fragShaderModule, nullptr);
		vkDestroyShaderModule(pimpl->device, vertShaderModule, nullptr);

		return pipeline;

	}

	void GFXDevice::destroyPipeline(const gfx::Pipeline* pipeline)
	{

		vkDestroyPipeline(pimpl->device, pipeline->handle, nullptr);
		vkDestroyPipelineLayout(pimpl->device, pipeline->layout, nullptr);

		vkDestroyDescriptorPool(pimpl->device, pipeline->descriptorPool, nullptr);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			destroyBuffer(pipeline->uniformBuffers[i]);
		}

		delete pipeline;
	}

	void GFXDevice::updateUniformBuffer(const gfx::Pipeline* pipeline, const void* data, size_t size, uint32_t offset)
	{
		assert(size <= pipeline->uniformBuffers[0]->size);

		[[maybe_unused]] VkResult res;

		for (gfx::Buffer* buffer : pipeline->uniformBuffers) {
			void* uniformDest = nullptr;
			res = vmaMapMemory(pimpl->allocator, buffer->allocation, &uniformDest);
			assert(res == VK_SUCCESS);
			memcpy((uint8_t*)uniformDest + offset, data, size);
			vmaUnmapMemory(pimpl->allocator, buffer->allocation);
		}
		
	}

	gfx::Buffer* GFXDevice::createBuffer(gfx::BufferType type, uint64_t size, const void* data)
	{
		[[maybe_unused]] VkResult res;

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

			res = vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr);
			assert(res == VK_SUCCESS);

			void* dataDest;
			res = vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest);
			assert(res == VK_SUCCESS);
			memcpy(dataDest, data, out->size);
			vmaUnmapMemory(pimpl->allocator, stagingAllocation);
		}

		// create the actual buffer on the GPU
		{
			VkBufferCreateInfo gpuBufferInfo{};
			gpuBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			gpuBufferInfo.size = out->size;
			gpuBufferInfo.usage = vkinternal::getBufferUsageFlag(type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			gpuBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			gpuBufferInfo.flags = 0;
			
			VmaAllocationCreateInfo gpuAllocationInfo{};
			gpuAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			gpuAllocationInfo.flags = 0;

			res = vmaCreateBuffer(pimpl->allocator, &gpuBufferInfo, &gpuAllocationInfo, &out->buffer, &out->allocation, nullptr);
			assert(res == VK_SUCCESS);
		}

		// copy the data from the staging buffer to the gpu buffer
		copyBuffer(pimpl->device, pimpl->commandPool, pimpl->gfxQueue.handle, stagingBuffer, out->buffer, out->size);

		// destroy staging buffer
		vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);

		return out;
	}

	void GFXDevice::destroyBuffer(const gfx::Buffer* buffer)
	{
		vmaDestroyBuffer(pimpl->allocator, buffer->buffer, buffer->allocation);
		delete buffer;
	}

	gfx::Texture* GFXDevice::createTexture(
		const void* imageData,
		uint32_t width,
		uint32_t height,
		gfx::TextureFilter minFilter,
		gfx::TextureFilter magFilter,
		gfx::MipmapSetting mipmapSetting,
		bool useAnisotropy)
	{
		auto out = new gfx::Texture;

		[[maybe_unused]] VkResult res;

		size_t imageSize = width * height * 4;

		if (mipmapSetting == gfx::MipmapSetting::OFF) {
			out->mipLevels = 1;
		} else {
			out->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}

		// first load image into staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		{
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = imageSize;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			stagingBufferInfo.flags = 0;

			VmaAllocationCreateInfo stagingAllocInfo{};
			stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			res = vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr);
			assert(res == VK_SUCCESS);

			void* dataDest;
			res = vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest);
			assert(res == VK_SUCCESS);
			memcpy(dataDest, imageData, imageSize);
			vmaUnmapMemory(pimpl->allocator, stagingAllocation);
		}

		// create the image
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = out->mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo imageAllocInfo{};
		imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		res = vmaCreateImage(pimpl->allocator, &imageInfo, &imageAllocInfo, &out->image, &out->alloc, nullptr);
		assert(res == VK_SUCCESS);

		// transition the image layout
		{
			VkCommandBuffer commandBuffer = beginOneTimeCommands(pimpl->device, pimpl->commandPool);

			// begin cmd buffer

			cmdTransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, out->mipLevels, out->image);

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;

			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			// Mipmap generation handles the transition to SHADER_READ_ONLY_OPTIMAL
			cmdGenerateMipmaps(commandBuffer, out->image, width, height, out->mipLevels);

			// end cmd buffer
			endOneTimeCommands(pimpl->device, pimpl->commandPool, commandBuffer, pimpl->gfxQueue.handle);

		}

		// destroy staging buffer
		vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);

		// create image view
		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = out->image;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageViewInfo.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = out->mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		};
		
		res = vkCreateImageView(pimpl->device, &imageViewInfo, nullptr, &out->imageView);
		assert(res == VK_SUCCESS);

		VkFilter magFilterInternal = vkinternal::getTextureFilter(magFilter);
		VkFilter minFilterInternal = vkinternal::getTextureFilter(minFilter);

		// create texture sampler
		{

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = magFilterInternal;
			samplerInfo.minFilter = minFilterInternal;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			if (useAnisotropy) {
				samplerInfo.anisotropyEnable = VK_TRUE;
			} else {
				samplerInfo.anisotropyEnable = VK_FALSE;
			}
			samplerInfo.maxAnisotropy = pimpl->maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			if (mipmapSetting == gfx::MipmapSetting::LINEAR) {
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			} else {
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			}
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = static_cast<float>(out->mipLevels);
			samplerInfo.mipLodBias = 0.0f;

			res = vkCreateSampler(pimpl->device, &samplerInfo, nullptr, &out->sampler);
			assert(res == VK_SUCCESS);
		}

		// create descriptor pools
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = FRAMES_IN_FLIGHT;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = FRAMES_IN_FLIGHT;
		res = vkCreateDescriptorPool(pimpl->device, &poolInfo, nullptr, &out->pool);
		assert(res == VK_SUCCESS);

		std::array<VkDescriptorSetLayout, FRAMES_IN_FLIGHT> layouts{};
		layouts.fill(pimpl->samplerSetLayout);
		VkDescriptorSetAllocateInfo dSetAllocInfo{};
		dSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dSetAllocInfo.descriptorPool = out->pool;
		dSetAllocInfo.descriptorSetCount = FRAMES_IN_FLIGHT;
		dSetAllocInfo.pSetLayouts = layouts.data();
		res = vkAllocateDescriptorSets(pimpl->device, &dSetAllocInfo, out->descriptorSets.data());
		assert(res == VK_SUCCESS);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = out->imageView;
			imageInfo.sampler = out->sampler;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = out->descriptorSets[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(pimpl->device, 1, &descriptorWrite, 0, nullptr);
		}

		return out;
	}

	void GFXDevice::destroyTexture(const gfx::Texture* texture)
	{
		vkDestroyDescriptorPool(pimpl->device, texture->pool, nullptr);
		vkDestroySampler(pimpl->device, texture->sampler, nullptr);
		vkDestroyImageView(pimpl->device, texture->imageView, nullptr);
		vmaDestroyImage(pimpl->allocator, texture->image, texture->alloc);
	}

	void GFXDevice::waitIdle()
	{
		vkDeviceWaitIdle(pimpl->device);
	}

}

#endif
