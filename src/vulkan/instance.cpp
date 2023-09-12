#include <vector>
#include <stdexcept>
#include <string>
#include <assert.h>

#include <SDL2/SDL_vulkan.h>

#include "util.h"
#include "config.h"
#include "log.h"

#include "instance.h"

namespace engine {

	static std::vector<const char*> getWindowExtensions(SDL_Window* window)
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

	static const char* getValidationLayer()
	{
		const char* const VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

		[[maybe_unused]] VkResult res;

		uint32_t layerCount;
		res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		assert(res == VK_SUCCESS);
		std::vector<VkLayerProperties> layers(layerCount);
		res = vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
		assert(res == VK_SUCCESS);

		// find validation layer and print all layers to log
		for (const auto& layer : layers) {
			if (strncmp(layer.layerName, VALIDATION_LAYER_NAME, 256) == 0) {
				return VALIDATION_LAYER_NAME;
			}
		}

		LOG_WARN("Unable to find validation layer!");

		return nullptr;

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
			LOG_DEBUG("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			LOG_INFO("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			LOG_WARN("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			LOG_ERROR("VULKAN MESSAGE{}: ID: {} MSG: {}", msgType, pCallbackData->pMessageIdName, pCallbackData->pMessage);
			break;
		default:
			break;
		}
		return VK_FALSE;
	}

	static VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo(MessageSeverity validationLevel)
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

		switch (validationLevel) {
		case MessageSeverity::SEV_VERBOSE:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
			[[fallthrough]];
		case MessageSeverity::SEV_INFO:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
			[[fallthrough]];
		case MessageSeverity::SEV_WARNING:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
			[[fallthrough]];
		case MessageSeverity::SEV_ERROR:
			debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			[[fallthrough]];
		default:
			break;
		}

		return debugMessengerInfo;
	}

	Instance createVulkanInstance(SDL_Window* window, const char* appName, const char* appVersion, bool useValidation, MessageSeverity validationLevel)
	{
		Instance instance;

		// get the both the engine and application versions
		int appVersionMajor = 0, appVersionMinor = 0, appVersionPatch = 0;
		VersionFromCharArray(appVersion, &appVersionMajor, &appVersionMinor, &appVersionPatch);
		int engineVersionMajor = 0, engineVersionMinor = 0, engineVersionPatch = 0;
		VersionFromCharArray(ENGINE_VERSION, &engineVersionMajor, &engineVersionMinor, &engineVersionPatch);

		VkApplicationInfo applicationInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = appName,
			.applicationVersion = VK_MAKE_VERSION(appVersionMajor, appVersionMinor, appVersionPatch),
			.pEngineName = "engine",
			.engineVersion = VK_MAKE_VERSION(engineVersionMajor, engineVersionMinor, engineVersionPatch),
			.apiVersion = VK_API_VERSION_1_3,
		};

		const std::vector<const char*> windowExtensions = getWindowExtensions(window);
		std::vector<const char*> instanceExtensionsToUse = windowExtensions;

		instanceExtensionsToUse.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

		const char* validationLayer = nullptr;
		if (useValidation) {
			validationLayer = getValidationLayer();
		}

		if (validationLayer) instanceExtensionsToUse.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = getDebugMessengerCreateInfo(validationLevel);

		VkInstanceCreateInfo instanceInfo{};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		if (validationLayer) {
			instanceInfo.pNext = &debugMessengerInfo;
		}
		instanceInfo.flags = 0;
		instanceInfo.pApplicationInfo = &applicationInfo;
		if (validationLayer) {
			instanceInfo.enabledLayerCount = 1;
			instanceInfo.ppEnabledLayerNames = &validationLayer;
		}
		instanceInfo.enabledExtensionCount = (uint32_t)instanceExtensionsToUse.size();
		instanceInfo.ppEnabledExtensionNames = instanceExtensionsToUse.data();

		VkResult res;
		res = vkCreateInstance(&instanceInfo, nullptr, &instance.instance);
		if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
			throw std::runtime_error("The graphics driver is incompatible with vulkan");
		}
		else if (res != VK_SUCCESS) {
			throw std::runtime_error("vkCreateInstance failed: " + std::to_string(res));
		}

		volkLoadInstanceOnly(instance.instance);

		// create the debug messenger
		if (useValidation) {
			VkDebugUtilsMessengerCreateInfoEXT createInfo = getDebugMessengerCreateInfo(validationLevel);
			res = vkCreateDebugUtilsMessengerEXT(instance.instance, &createInfo, nullptr, &instance.debugMessenger);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("vkCreateDebugUtilsMessengerExt failed: " + std::to_string(res));
			}
		}

		return instance;
	}

	void destroyVulkanInstance(Instance instance)
	{
		if (instance.debugMessenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(instance.instance, instance.debugMessenger, nullptr);
		}
		vkDestroyInstance(instance.instance, nullptr);
	}

}
