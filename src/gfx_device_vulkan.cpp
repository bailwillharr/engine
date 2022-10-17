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

namespace engine {

	// singleton variable
	static GFXDevice* s_gfx_device_instance = nullptr;

	// structures

	struct LayerInfo {
		std::vector<VkLayerProperties> layersAvailable{};
		std::optional<std::vector<VkLayerProperties>::iterator> validationLayer;
	};

	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR caps{};
		std::vector<VkSurfaceFormatKHR> formats{};
		std::vector<VkPresentModeKHR> presentModes{};
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
		VkFormat format;

		std::vector<VkImage> images{};
		std::vector<VkImageView> imageViews{};
	};

	// enums

	enum class QueueFlags : uint32_t {
		GRAPHICS =	(1 << 0),
		TRANSFER =	(1 << 1),
		COMPUTE =	(1 << 2),
	};

	// functions

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

	static void createSwapchain(VkDevice device, const std::vector<Queue> queues, SDL_Window* window, VkSurfaceKHR surface, const SwapchainSupportDetails& supportDetails, Swapchain* swapchain)
	{
		VkResult res;

		VkSurfaceFormatKHR chosenSurfaceFormat = supportDetails.formats[0];

		for (const auto& format : supportDetails.formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				chosenSurfaceFormat = format; // prefer using srgb non linear colors
			}
		}

		VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& presMode : supportDetails.presentModes) {
			if (presMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				chosenPresentMode = presMode; // this mode allows uncapped FPS while also avoiding screen tearing
			}
		}

		VkExtent2D chosenSwapExtent{};

		if (supportDetails.caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			chosenSwapExtent = supportDetails.caps.currentExtent;
		}
		else {
			// if fb size isn't already found, get it from SDL
			int width, height;
			SDL_Vulkan_GetDrawableSize(window, &width, &height);

			chosenSwapExtent.width = static_cast<uint32_t>(width);
			chosenSwapExtent.height = static_cast<uint32_t>(height);

			chosenSwapExtent.width = std::clamp(
				chosenSwapExtent.width,
				supportDetails.caps.minImageExtent.width, supportDetails.caps.maxImageExtent.width);
			chosenSwapExtent.height = std::clamp(
				chosenSwapExtent.height,
				supportDetails.caps.minImageExtent.height, supportDetails.caps.maxImageExtent.height);
		}

		uint32_t imageCount = supportDetails.caps.minImageCount + 1;
		if (supportDetails.caps.maxImageCount > 0 && imageCount > supportDetails.caps.maxImageCount) {
			imageCount = supportDetails.caps.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = nullptr,
			.flags = 0,
			.surface = surface,
			.minImageCount = imageCount,
			.imageFormat = chosenSurfaceFormat.format,
			.imageColorSpace = chosenSurfaceFormat.colorSpace,
			.imageExtent = chosenSwapExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.preTransform = supportDetails.caps.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = chosenPresentMode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE,

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

		// get all the image handles
		uint32_t swapchainImageCount = 0;
		res = vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchainImageCount, nullptr);
		assert(res == VK_SUCCESS);
		swapchain->images.resize(swapchainImageCount);
		res = vkGetSwapchainImagesKHR(device, swapchain->swapchain, &swapchainImageCount, swapchain->images.data());
		assert(res == VK_SUCCESS);

		swapchain->format = chosenSurfaceFormat.format;
		swapchain->extent = chosenSwapExtent;

		// create image views
		swapchain->imageViews.clear();
		for (VkImage image : swapchain->images) {

			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.pNext = nullptr;
			createInfo.image = image;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapchain->format;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VkImageView imageView;
			res = vkCreateImageView(device, &createInfo, nullptr, &imageView);
			assert(res == VK_SUCCESS);

			swapchain->imageViews.push_back(imageView);

		}

	}

	struct GFXDevice::Impl {
		
		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
		
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		SwapchainSupportDetails swapchainSupportDetails{}; // available capabilities, formats, and present modes

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE;
		
		std::vector<Queue> queues{};
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

		VmaAllocator allocator = nullptr;

		Swapchain swapchain{};

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		// ensure there is only one instance of this class
		if (s_gfx_device_instance != nullptr) {
			throw std::runtime_error("Multiple gfxdevice classes cannot be created");
		}
		s_gfx_device_instance = this;

		pimpl = std::make_unique<Impl>();

		VkResult res;

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



				// get swapchain support details:

				// get surface capabilities
				res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, pimpl->surface, &pimpl->swapchainSupportDetails.caps);
				assert(res == VK_SUCCESS);

				// check there is at least one supported surface format
				uint32_t surfaceFormatCount = 0;
				res = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, pimpl->surface, &surfaceFormatCount, nullptr);
				assert(res == VK_SUCCESS);
				if (surfaceFormatCount == 0) {
					continue;
				}
				pimpl->swapchainSupportDetails.formats.resize(surfaceFormatCount);
				res = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, pimpl->surface, &surfaceFormatCount, pimpl->swapchainSupportDetails.formats.data());
				assert(res == VK_SUCCESS);

				// check there is at least one supported present mode
				uint32_t surfacePresentModeCount = 0;
				res = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, pimpl->surface, &surfacePresentModeCount, nullptr);
				assert(res == VK_SUCCESS);
				if (surfacePresentModeCount == 0) {
					continue;
				}
				pimpl->swapchainSupportDetails.presentModes.resize(surfacePresentModeCount);
				res = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, pimpl->surface, &surfacePresentModeCount, pimpl->swapchainSupportDetails.presentModes.data());
				assert(res == VK_SUCCESS);



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

			TRACE("Supported present modes:");
			for (const auto& presMode : pimpl->swapchainSupportDetails.presentModes) {
				switch (presMode) {
				case VK_PRESENT_MODE_IMMEDIATE_KHR:
					TRACE("\tVK_PRESENT_MODE_IMMEDIATE_KHR");
					break;
				case VK_PRESENT_MODE_MAILBOX_KHR:
					TRACE("\tVK_PRESENT_MODE_MAILBOX_KHR");
					break;
				case VK_PRESENT_MODE_FIFO_KHR:
					TRACE("\tVK_PRESENT_MODE_FIFO_KHR");
					break;
				case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
					TRACE("\tVK_PRESENT_MODE_FIFO_RELAXED_KHR");
					break;
				case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
					TRACE("\tVK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR");
					break;
				case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
					TRACE("\tVK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR");
					break;
				default:
					TRACE("\tUNKNOWN DISPLAY MODE");
					break;
				}
			}



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

			Queue gfxQueue = getQueueSupporting(pimpl->queues, QueueFlags::GRAPHICS);

			VkCommandPoolCreateInfo gfxCmdPoolInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = gfxQueue.familyIndex,
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
		createSwapchain(pimpl->device, pimpl->queues, window, pimpl->surface, pimpl->swapchainSupportDetails, &pimpl->swapchain);

	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");

		for (VkImageView view : pimpl->swapchain.imageViews) {
			vkDestroyImageView(pimpl->device, view, nullptr);
		}
		vkDestroySwapchainKHR(pimpl->device, pimpl->swapchain.swapchain, nullptr);

		vmaDestroyAllocator(pimpl->allocator);

		vkDestroyCommandPool(pimpl->device, pimpl->commandPool, nullptr);
		vkDestroyDevice(pimpl->device, nullptr);
		vkDestroySurfaceKHR(pimpl->instance, pimpl->surface, nullptr);
		vkDestroyDebugUtilsMessengerEXT(pimpl->instance, pimpl->debugMessenger, nullptr);
		vkDestroyInstance(pimpl->instance, nullptr);

		s_gfx_device_instance = nullptr;
	}

	void GFXDevice::draw()
	{
		
	}

	bool GFXDevice::createBuffer(const gfx::BufferDesc& desc, const void* data, gfx::BufferHandle* out)
	{
		return false;
	}

}

#endif
