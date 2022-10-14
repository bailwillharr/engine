// The implementation of the graphics layer using Vulkan 1.3

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

namespace engine {

	// structures

	struct LayerInfo {
		std::vector<VkLayerProperties> layersAvailable{};
		std::optional<std::vector<VkLayerProperties>::iterator> validationLayer;
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

	struct GFXDevice::Impl {
		
		VkInstance m_instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		m_pimpl = std::make_unique<Impl>();

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

		res = vkCreateInstance(&instanceInfo, nullptr, &m_pimpl->m_instance);
		if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
			throw std::runtime_error("The graphics driver is incompatible with vulkan");
		}
		assert(res == VK_SUCCESS);

		

		// load the instance functions
		volkLoadInstanceOnly(m_pimpl->m_instance);



		// create the debug messenger
		{
			VkDebugUtilsMessengerCreateInfoEXT createInfo = getDebugMessengerCreateInfo();

			VkResult res = vkCreateDebugUtilsMessengerEXT(m_pimpl->m_instance, &createInfo, nullptr, &m_pimpl->m_debugMessenger);
			assert(res == VK_SUCCESS);
		}

	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");

		vkDestroyDebugUtilsMessengerEXT(m_pimpl->m_instance, m_pimpl->m_debugMessenger, nullptr);
		vkDestroyInstance(m_pimpl->m_instance, nullptr);
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
