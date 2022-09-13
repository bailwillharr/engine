#ifdef ENGINE_BUILD_VULKAN

#include "gfx_device.hpp"

#include "config.h"

#include "window.hpp"

#include "log.hpp"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#include <assert.h>

#include <iostream>
#include <optional>

namespace engine::gfx {

	class Device::Impl {
		friend Device;

		VkInstance m_instance;
#ifndef NDEBUG
		VkDebugUtilsMessengerEXT m_debugMessenger;
#endif

		static constexpr const char * VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
		static constexpr VkDebugUtilsMessageSeverityFlagBitsEXT MESSAGE_LEVEL = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

		std::vector<VkLayerProperties> m_layersAvailable{};
		std::optional<std::vector<VkLayerProperties>::iterator> m_validationLayer;



		void findAvailableLayers()
		{
			VkResult res;

			uint32_t layerCount;
			res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
			assert(res == VK_SUCCESS);
			m_layersAvailable.resize(layerCount);
			res = vkEnumerateInstanceLayerProperties(&layerCount, m_layersAvailable.data());
			assert(res == VK_SUCCESS);

#ifndef NDEBUG
			for (auto it = m_layersAvailable.begin(); it != m_layersAvailable.end(); it++) {
				TRACE("Found Vulkan layer: {}, {}", it->layerName, it->description);
				if (strncmp(it->layerName, VALIDATION_LAYER_NAME, 256) == 0) {
					m_validationLayer = it;
				}
			}
			if (m_validationLayer.has_value() == false) {
				WARN("The validation layer was not found. Continuing.");
			}
#endif
		}

		void createInstance(AppInfo appInfo, const std::vector<const char*>& windowExtensions)
		{
			VkResult res;

			int appVersionMajor, appVersionMinor, appVersionPatch;
			assert(versionFromCharArray(appInfo.version, &appVersionMajor, &appVersionMinor, &appVersionPatch));
			int engineVersionMajor, engineVersionMinor, engineVersionPatch;
			assert(versionFromCharArray(ENGINE_VERSION, &engineVersionMajor, &engineVersionMinor, &engineVersionPatch));

			VkApplicationInfo applicationInfo {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pNext = nullptr,
				.pApplicationName = appInfo.name,
				.applicationVersion = VK_MAKE_VERSION(appVersionMajor, appVersionMinor, appVersionPatch),
				.pEngineName = "engine",
				.engineVersion = VK_MAKE_VERSION(engineVersionMajor, engineVersionMinor, engineVersionPatch),
				.apiVersion = VK_API_VERSION_1_0,
			};

			// make a list of all extensions to use
			std::vector<const char*> extensions{};
			extensions.insert(extensions.end(), windowExtensions.begin(), windowExtensions.end());
#ifndef NDEBUG
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

			std::vector<const char*> layers{};

#ifndef NDEBUG
			if (m_validationLayer.has_value())
				layers.push_back(m_validationLayer.value()->layerName);
#endif

			VkInstanceCreateInfo instanceInfo {
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.pApplicationInfo = &applicationInfo,
				.enabledLayerCount = (uint32_t)layers.size(),
				.ppEnabledLayerNames = layers.data(),
				.enabledExtensionCount = (uint32_t)extensions.size(),
				.ppEnabledExtensionNames = extensions.data(),
			};
	
			res = vkCreateInstance(&instanceInfo, nullptr, &m_instance);
			if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
				CRITICAL("The graphics driver is incompatible with vulkan");
				throw std::runtime_error("Graphics driver is incompatible with Vulkan");
			}
			assert(res == VK_SUCCESS);
		}

#ifndef NDEBUG
		static VkBool32 debugMessenger(
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
#endif

		void createDebugMessenger()
		{
#ifndef NDEBUG
			VkResult res;

			if (m_validationLayer.has_value() == false) return;

			VkDebugUtilsMessengerCreateInfoEXT createInfo {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext = nullptr,
				.flags = 0,
				.messageSeverity = 0,
				.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = debugMessenger,
				.pUserData = nullptr,
			};

			switch (MESSAGE_LEVEL) {
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
				createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
				// fall-through
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
				createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
				// fall-through
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
				createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
				// fall-through
			case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
				createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				// fall-through
			default:
				break;
			}

			res = vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
			assert(res == VK_SUCCESS);
#endif
		}

	};

	

	Device::Device(AppInfo appInfo, const Window& window) : pimpl(std::make_unique<Impl>())
	{
		pimpl->findAvailableLayers();
		pimpl->createInstance(appInfo, window.getRequiredVulkanExtensions());
		pimpl->createDebugMessenger();
	}

	Device::~Device()
	{
		vkDestroyInstance(pimpl->m_instance, nullptr);
#ifndef NDEBUG
		vkDestroyDebugUtilsMessengerEXT(pimpl->m_instance, pimpl->m_debugMessenger, nullptr);
#endif
	}

}

#endif
