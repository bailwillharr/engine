// The implementation of the graphics layer using Vulkan 1.3.
// This uses SDL specific code

#ifdef ENGINE_BUILD_VULKAN

#include "gfx_device.hpp"
#include "util.hpp"
#include "config.h"
#include "log.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <SDL_vulkan.h>

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>
#include <queue>

namespace engine {

	// structures and enums

	struct LayerInfo {
		std::vector<VkLayerProperties> layersAvailable{};
		std::optional<std::vector<VkLayerProperties>::iterator> validationLayer;
	};

	struct Queue {
		uint32_t familyIndex;
		uint32_t queueIndex;
		bool supportsGraphics = false;
		bool supportsTransfer = false;
		bool supportsCompute = false;

		VkQueue handle;
	};

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;

		VkExtent2D extent;
		VkSurfaceFormatKHR surfaceFormat;
		VkPresentModeKHR presentMode;

		std::vector<VkImage> images{};
		std::vector<VkImageView> imageViews{};
		std::vector<VkFramebuffer> framebuffers{};

		VkQueue activeQueue{};

		VkRenderPass renderpass;

		VkSemaphore acquireSemaphore = VK_NULL_HANDLE; // waits until the image is available
		VkSemaphore releaseSemaphore = VK_NULL_HANDLE; // waits until rendering finishes
	};

	struct Pipeline {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipeline handle = VK_NULL_HANDLE;
	};

	enum class QueueFlags : uint32_t {
		GRAPHICS =	(1 << 0),
		TRANSFER =	(1 << 1),
		COMPUTE =	(1 << 2),
	};

	// handles
	
	struct gfx::VertexBuffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		uint32_t size;
	};



	// enum converters

	namespace vkinternal {

		static VkFormat getVertexAttribFormat(gfx::VertexAttribFormat fmt)
		{
			switch (fmt) {
			case gfx::VertexAttribFormat::VEC2:
				return VK_FORMAT_R32G32_SFLOAT;
			case gfx::VertexAttribFormat::VEC3:
				return VK_FORMAT_R32G32B32_SFLOAT;
			}
			throw std::runtime_error("Unknown vertex attribute format");
		}
	}

	// functions

	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (file.is_open() == false) {
			throw std::runtime_error("Unable to open file " + filename);
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0);
		file.read(buffer.data(), buffer.size());
		file.close();
		return buffer;
	}

	static std::vector<const char*> getRequiredVulkanExtensions(SDL_Window* window)
	{
		SDL_bool res;

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
		VkResult res;

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

		std::string msgType{};

		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			msgType += " (GENERAL)";
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			msgType += " (PERF.)";
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			msgType += " (VALID.)";

		switch (messageSeverity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			TRACE("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
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

	// returns the index of the queue supporting the requested flags
	static Queue getQueueSupporting(const std::vector<Queue> queues, QueueFlags flags)
	{
		uint32_t bitmask = static_cast<uint32_t>(flags);

		for (int i = 0; i < queues.size(); i++) {

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

	// This is called not just on initialisation, but also when the window is resized.
	static void createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<Queue> queues, SDL_Window* window, VkSurfaceKHR surface, Swapchain* swapchain)
	{
		VkResult res;

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

		for (const auto& presMode : presentModes) {
			if (presMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				//swapchain->presentMode = presMode; // this mode allows uncapped FPS while also avoiding screen tearing
			}
		}

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

		// create the render pass
		if (swapchain->renderpass == VK_NULL_HANDLE) {
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = swapchain->surfaceFormat.format;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = &colorAttachment;
			createInfo.subpassCount = 1;
			createInfo.pSubpasses = &subpass;

			createInfo.dependencyCount = 1;
			createInfo.pDependencies = &dependency;

			//if (swapchain->renderpass != VK_NULL_HANDLE) {
			//	vkDestroyRenderPass(device, swapchain->renderpass, nullptr);
			//	swapchain->renderpass = VK_NULL_HANDLE;
			//}
			res = vkCreateRenderPass(device, &createInfo, nullptr, &swapchain->renderpass);
		}

		// create image views and framebuffers

		swapchain->imageViews.resize(swapchain->images.size());
		swapchain->framebuffers.resize(swapchain->images.size());
		for (int i = 0; i < swapchain->images.size(); i++) {

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

			VkImageView attachments[] = {
				swapchain->imageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = swapchain->renderpass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapchain->extent.width;
			framebufferInfo.height = swapchain->extent.height;
			framebufferInfo.layers = 1;

			res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain->framebuffers[i]);
			assert(res == VK_SUCCESS);

		}

		// create the swapchain semaphores
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (swapchain->acquireSemaphore == VK_NULL_HANDLE) {
			res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain->acquireSemaphore);
			assert(res == VK_SUCCESS);
		}
		if (swapchain->releaseSemaphore == VK_NULL_HANDLE) {
			res = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchain->releaseSemaphore);
			assert(res == VK_SUCCESS);
		}

	}

	static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;
	}



	// class definitions

	struct GFXDevice::Impl {
		
		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

		SDL_Window* window = nullptr;
		
		VkSurfaceKHR surface = VK_NULL_HANDLE;

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		
		std::vector<Queue> queues{};
		Queue gfxQueue;
		Queue presentQueue;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

		VmaAllocator allocator = nullptr;

		Swapchain swapchain{};

		VkFence inFlightFence = VK_NULL_HANDLE;

		Pipeline pipeline{};

		std::queue<const gfx::VertexBuffer*> drawQueue{};

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		pimpl = std::make_unique<Impl>();

		VkResult res;

		pimpl->window = window;

		// initialise vulkan

		res = volkInitialize();
		if (res == VK_ERROR_INITIALIZATION_FAILED) {
			throw std::runtime_error("Unable to load vulkan, is it installed?");
		}

		assert(res == VK_SUCCESS);

		uint32_t vulkanVersion = volkGetInstanceVersion();
		if (vulkanVersion < VK_MAKE_VERSION(1, 3, 0)) {
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
		assert(versionFromCharArray(appVersion, &appVersionMajor, &appVersionMinor, &appVersionPatch));
		int engineVersionMajor = 0, engineVersionMinor = 0, engineVersionPatch = 0;
		assert(versionFromCharArray(ENGINE_VERSION, &engineVersionMajor, &engineVersionMinor, &engineVersionPatch));

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



#ifndef NDEBUG
		for (const char* ext : extensions) {
			TRACE("Using Vulkan instance extension: {}", ext);
		}
#endif

		res = vkCreateInstance(&instanceInfo, nullptr, &pimpl->instance);
		if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
			throw std::runtime_error("The graphics driver is incompatible with vulkan");
		}
		assert(res == VK_SUCCESS);



		// load the instance functions
		volkLoadInstanceOnly(pimpl->instance);



		// create the debug messenger
		{
			VkDebugUtilsMessengerCreateInfoEXT createInfo = getDebugMessengerCreateInfo();

			VkResult res = vkCreateDebugUtilsMessengerEXT(pimpl->instance, &createInfo, nullptr, &pimpl->debugMessenger);
			assert(res == VK_SUCCESS);
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

				for (const auto& extToFind : requiredDeviceExtensions) {
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

				pimpl->physicalDevice = dev;
				break;

			} // end for()

			if (pimpl->physicalDevice == VK_NULL_HANDLE) {
				throw std::runtime_error("No suitable Vulkan physical device found");
			}

			VkPhysicalDeviceProperties devProps;
			vkGetPhysicalDeviceProperties(pimpl->physicalDevice, &devProps);
			INFO("Selected physical device: {}", devProps.deviceName);



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
						TRACE("GRAPHICS:");
						graphicsFamilyIndex = i;
					}
					if (transferFamilyIndex.has_value() == false && family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
						TRACE("TRANSFER:");
						transferFamilyIndex = i;
					}
					if (computeFamilyIndex.has_value() == false && family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
						TRACE("COMPUTE:");
						computeFamilyIndex = i;
					}
					TRACE("\t\ti = {}\t\tcount = {}", i, family.queueCount);
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
				if (graphicsFamilyIndex == family) newQueue.supportsGraphics = true;
				if (transferFamilyIndex == family) newQueue.supportsTransfer = true;
				if (computeFamilyIndex == family) newQueue.supportsCompute = true;

				TRACE("Creating queue from family {}", family);
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

			VkDeviceCreateInfo deviceCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.queueCreateInfoCount = (uint32_t)queueCreateInfos.size(),
				.pQueueCreateInfos = queueCreateInfos.data(),
				// IGNORED: .enabledLayerCount
				// IGNORED: .ppEnabledLayerNames
				.enabledExtensionCount = (uint32_t)requiredDeviceExtensions.size(),
				.ppEnabledExtensionNames = requiredDeviceExtensions.data(),
				.pEnabledFeatures = nullptr,
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
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = pimpl->gfxQueue.familyIndex,
			};

			res = vkCreateCommandPool(pimpl->device, &gfxCmdPoolInfo, nullptr, &pimpl->commandPool);
			assert(res == VK_SUCCESS);

			VkCommandBufferAllocateInfo gfxCmdBufInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = pimpl->commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};

			res = vkAllocateCommandBuffers(pimpl->device, &gfxCmdBufInfo, &pimpl->commandBuffer);
			assert(res == VK_SUCCESS);
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

			VkResult res = vmaCreateAllocator(&createInfo, &pimpl->allocator);
			assert(res == VK_SUCCESS);
		}



		// Now make the swapchain
		createSwapchain(pimpl->device, pimpl->physicalDevice, pimpl->queues, window, pimpl->surface, &pimpl->swapchain);



		// for testing purposes, create a pipeline with a simple shader
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		fenceInfo.pNext = nullptr;
		res = vkCreateFence(pimpl->device, &fenceInfo, nullptr, &pimpl->inFlightFence);

	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");

		if (pimpl->pipeline.handle != VK_NULL_HANDLE) {
			vkDestroyPipeline(pimpl->device, pimpl->pipeline.handle, nullptr);
			vkDestroyPipelineLayout(pimpl->device, pimpl->pipeline.layout, nullptr);
		}

		vkDestroyFence(pimpl->device, pimpl->inFlightFence, nullptr);

		vkDestroySemaphore(pimpl->device, pimpl->swapchain.releaseSemaphore, nullptr);
		vkDestroySemaphore(pimpl->device, pimpl->swapchain.acquireSemaphore, nullptr);
		for (VkImageView view : pimpl->swapchain.imageViews) {
			vkDestroyImageView(pimpl->device, view, nullptr);
		}
		for (VkFramebuffer fb : pimpl->swapchain.framebuffers) {
			vkDestroyFramebuffer(pimpl->device, fb, nullptr);
		}
		vkDestroyRenderPass(pimpl->device, pimpl->swapchain.renderpass, nullptr);
		vkDestroySwapchainKHR(pimpl->device, pimpl->swapchain.swapchain, nullptr);

		vmaDestroyAllocator(pimpl->allocator);

		vkDestroyCommandPool(pimpl->device, pimpl->commandPool, nullptr);
		vkDestroyDevice(pimpl->device, nullptr);
		vkDestroySurfaceKHR(pimpl->instance, pimpl->surface, nullptr);
		vkDestroyDebugUtilsMessengerEXT(pimpl->instance, pimpl->debugMessenger, nullptr);
		vkDestroyInstance(pimpl->instance, nullptr);
	}

	void GFXDevice::drawBuffer(const gfx::VertexBuffer* vb)
	{
		pimpl->drawQueue.push(vb);
	}

	void GFXDevice::draw()
	{
		VkResult res;

		res = vkWaitForFences(pimpl->device, 1, &pimpl->inFlightFence, VK_TRUE, UINT64_MAX);
		assert(res == VK_SUCCESS);
		res = vkResetFences(pimpl->device, 1, &pimpl->inFlightFence);
		assert(res == VK_SUCCESS);

		uint32_t imageIndex = 0;
		res = vkAcquireNextImageKHR(pimpl->device, pimpl->swapchain.swapchain, UINT64_MAX, pimpl->swapchain.acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (res == VK_ERROR_OUT_OF_DATE_KHR) {
			// recreate swapchain
			waitIdle();
			createSwapchain(pimpl->device, pimpl->physicalDevice, pimpl->queues, pimpl->window, pimpl->surface, &pimpl->swapchain);
			return;
		}
		else {
			assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
		}

		res = vkResetCommandBuffer(pimpl->commandBuffer, 0);
		assert(res == VK_SUCCESS);
		
		// now record command buffer
		{
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;
			res = vkBeginCommandBuffer(pimpl->commandBuffer, &beginInfo);
			assert(res == VK_SUCCESS);

			VkRenderPassBeginInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = pimpl->swapchain.renderpass;
			renderPassInfo.framebuffer = pimpl->swapchain.framebuffers[imageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = pimpl->swapchain.extent;

			VkClearValue clearColor{ {0.0f, 0.0f, 0.0f, 0.0f} };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;
			
			vkCmdBeginRenderPass(pimpl->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)pimpl->swapchain.extent.width;
			viewport.height = (float)pimpl->swapchain.extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(pimpl->commandBuffer, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = pimpl->swapchain.extent;
			vkCmdSetScissor(pimpl->commandBuffer, 0, 1, &scissor);

			// run queued draw calls

			vkCmdBindPipeline(pimpl->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pimpl->pipeline.handle);
			VkDeviceSize offsets[] = { 0 };
			while (pimpl->drawQueue.empty() == false) {

				const auto* buffer = pimpl->drawQueue.front();
				
				vkCmdBindVertexBuffers(pimpl->commandBuffer, 0, 1, &buffer->buffer, offsets);
				vkCmdDraw(pimpl->commandBuffer, buffer->size, 1, 0, 0);

				pimpl->drawQueue.pop();

			}

			vkCmdEndRenderPass(pimpl->commandBuffer);

			res = vkEndCommandBuffer(pimpl->commandBuffer);
			assert(res == VK_SUCCESS);
		}

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &pimpl->swapchain.acquireSemaphore;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &pimpl->commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &pimpl->swapchain.releaseSemaphore;

		res = vkQueueSubmit(pimpl->gfxQueue.handle, 1, &submitInfo, pimpl->inFlightFence);
		assert(res == VK_SUCCESS);

		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &pimpl->swapchain.releaseSemaphore;

		VkSwapchainKHR swapchains[] = { pimpl->swapchain.swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		res = vkQueuePresentKHR(pimpl->presentQueue.handle, &presentInfo);
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
			// recreate swapchain
			waitIdle();
			createSwapchain(pimpl->device, pimpl->physicalDevice, pimpl->queues, pimpl->window, pimpl->surface, &pimpl->swapchain);
		}
		else {
			assert(res == VK_SUCCESS);
		}
	}
	
	void GFXDevice::createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat)
	{

		VkResult res;

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

		auto vertShaderCode = readFile(vertShaderPath);
		auto fragShaderCode = readFile(fragShaderPath);
		INFO("Opened shader: {}", std::filesystem::path(vertShaderPath).filename().string());

		VkShaderModule vertShaderModule = createShaderModule(pimpl->device, vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(pimpl->device, fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";
		vertShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
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
		vertexInputInfo.vertexAttributeDescriptionCount = attribDescs.size();
		vertexInputInfo.pVertexAttributeDescriptions = attribDescs.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)pimpl->swapchain.extent.width;
		viewport.height = (float)pimpl->swapchain.extent.height;
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
		dynamicState.dynamicStateCount = dynamicStates.size();
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
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // ignored
		rasterizer.depthBiasClamp = 0.0f; // ignored
		rasterizer.depthBiasSlopeFactor = 0.0f; // ignored

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // ignored
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // ignored
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // ignored
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // ignored
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // ignored
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // ignored

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

		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		// everything is 0 because we're not using uniforms
		layoutInfo.setLayoutCount = 0;
		layoutInfo.pSetLayouts = nullptr;
		layoutInfo.pushConstantRangeCount = 0;
		layoutInfo.pPushConstantRanges = nullptr;

		res = vkCreatePipelineLayout(pimpl->device, &layoutInfo, nullptr, &pimpl->pipeline.layout);
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
		createInfo.pDepthStencilState = nullptr;
		createInfo.pColorBlendState = &colorBlending;
		createInfo.pDynamicState = &dynamicState;
		createInfo.layout = pimpl->pipeline.layout;
		createInfo.renderPass = pimpl->swapchain.renderpass;
		createInfo.subpass = 0;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		res = vkCreateGraphicsPipelines(pimpl->device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pimpl->pipeline.handle);
		assert(res == VK_SUCCESS);

		vkDestroyShaderModule(pimpl->device, fragShaderModule, nullptr);
		vkDestroyShaderModule(pimpl->device, vertShaderModule, nullptr);

	}

	gfx::VertexBuffer* GFXDevice::createVertexBuffer(uint32_t size, const void* vertices, const void* indices)
	{
		auto out = new gfx::VertexBuffer{};
		out->size = size;

		VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		createInfo.size = out->size;
		createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.flags = 0;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		VkResult res = vmaCreateBuffer(pimpl->allocator, &createInfo, &allocInfo, &out->buffer, &out->allocation, nullptr);
		assert(res == VK_SUCCESS);

		void* data;
		res = vmaMapMemory(pimpl->allocator, out->allocation, &data);
		assert(res == VK_SUCCESS);
		memcpy(data, vertices, out->size);
		vmaUnmapMemory(pimpl->allocator, out->allocation);

		return out;
	}

	void GFXDevice::destroyVertexBuffer(const gfx::VertexBuffer* buffer)
	{
		vmaDestroyBuffer(pimpl->allocator, buffer->buffer, buffer->allocation);
		delete buffer;
	}

	void GFXDevice::waitIdle()
	{
		vkDeviceWaitIdle(pimpl->device);
	}

}

#endif
