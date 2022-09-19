#ifdef ENGINE_BUILD_VULKAN

#include "gfx_device.hpp"

#include "config.h"
#include "log.hpp"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#include <SDL_vulkan.h>

#include <assert.h>

#include <iostream>
#include <optional>

namespace engine::gfx {

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

	class Device::Impl {

	public:
		Impl(AppInfo appInfo, SDL_Window* window)
		{
#ifdef NDEBUG
			findAvailableLayers(m_layerInfo, false);
#else
			findAvailableLayers(m_layerInfo, true);
#endif
			m_instance = std::make_shared<Instance>(appInfo, m_layerInfo, getRequiredVulkanExtensions(window));
			m_debugMessenger = std::make_unique<DebugMessenger>(m_instance);
		}
		~Impl()
		{
		}

	private:

//		VkSurfaceKHR m_surface;

		struct LayerInfo {

			static constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

			std::vector<VkLayerProperties> layersAvailable{};
			std::optional<std::vector<VkLayerProperties>::iterator> validationLayer;

		} m_layerInfo;

		static void findAvailableLayers(LayerInfo& layerInfo, bool useValidation)
		{
			VkResult res;

			uint32_t layerCount;
			res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
			assert(res == VK_SUCCESS);
			layerInfo.layersAvailable.resize(layerCount);
			res = vkEnumerateInstanceLayerProperties(&layerCount, layerInfo.layersAvailable.data());
			assert(res == VK_SUCCESS);

			if (useValidation == true) {
				// find validation layer and print all layers to log
				for (auto it = layerInfo.layersAvailable.begin(); it != layerInfo.layersAvailable.end(); it++) {
					if (strncmp(it->layerName, LayerInfo::VALIDATION_LAYER_NAME, 256) == 0) {
						layerInfo.validationLayer = it;
					}
				}
				if (layerInfo.validationLayer.has_value() == false) {
					CRITICAL("The validation layer was not found. Quitting.");
					throw std::runtime_error("Validation layer not found");
				}
			}
		}

		class Instance {

		public:
			Instance(AppInfo appInfo, const LayerInfo& layerInfo, const std::vector<const char*>& windowExtensions)
			{
				VkResult res;

				int appVersionMajor, appVersionMinor, appVersionPatch;
				assert(versionFromCharArray(appInfo.version, &appVersionMajor, &appVersionMinor, &appVersionPatch));
				int engineVersionMajor, engineVersionMinor, engineVersionPatch;
				assert(versionFromCharArray(ENGINE_VERSION, &engineVersionMajor, &engineVersionMinor, &engineVersionPatch));

				VkApplicationInfo applicationInfo{
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

				std::vector<const char*> layers{};

				if (layerInfo.validationLayer.has_value()) {
					layers.push_back(layerInfo.validationLayer.value()->layerName);
					extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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

				VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = DebugMessenger::getCreateInfo();

				if (layerInfo.validationLayer.has_value()) {
					instanceInfo.pNext = &debugMessengerCreateInfo;
				}

#ifndef NDEBUG
				for (const char* ext : extensions) {
					TRACE("Using Vulkan instance extension: {}", ext);
				}
#endif

				res = vkCreateInstance(&instanceInfo, nullptr, &m_handle);
				if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
					CRITICAL("The graphics driver is incompatible with vulkan");
					throw std::runtime_error("Graphics driver is incompatible with Vulkan");
				}
				assert(res == VK_SUCCESS);

				volkLoadInstance(m_handle);

			}

			~Instance()
			{
				INFO("DESTROYING INSTANCE...");
				vkDestroyInstance(m_handle, nullptr);
				INFO("DESTROYED INSTANCE.");
			}

			VkInstance getHandle()
			{
				return m_handle;
			}

		private:

			VkInstance m_handle;

		};

		class DebugMessenger {

		public:
			DebugMessenger(std::shared_ptr<Instance> instance) : m_instance(instance)
			{
				VkDebugUtilsMessengerCreateInfoEXT createInfo = getCreateInfo();

				VkResult res = vkCreateDebugUtilsMessengerEXT(instance->getHandle(), &createInfo, nullptr, &m_messengerHandle);
				assert(res == VK_SUCCESS);
			}

			~DebugMessenger()
			{
				INFO("DESTROYING MESSENGER...");
				vkDestroyDebugUtilsMessengerEXT(m_instance->getHandle(), m_messengerHandle, nullptr);
				INFO("DESTROYED MESSENGER.");
			}

			static constexpr VkDebugUtilsMessageSeverityFlagBitsEXT MESSAGE_LEVEL = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

			static VkDebugUtilsMessengerCreateInfoEXT getCreateInfo()
			{
				VkDebugUtilsMessengerCreateInfoEXT createInfo{
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

				return createInfo;
			}

		private:
			VkDebugUtilsMessengerEXT m_messengerHandle;
			std::shared_ptr<Instance> m_instance;

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

		};

		std::shared_ptr<Instance> m_instance;
		std::unique_ptr<DebugMessenger> m_debugMessenger;

/*

		void createSurface(SDL_Window* window)
		{
			if (SDL_Vulkan_CreateSurface(window, m_instance, &m_surface) == false) {
				CRITICAL("Unable to create window surface");
				throw std::runtime_error("Unable to create window surface");
			}
		}
*/	

	};

	Device::Device(AppInfo appInfo, SDL_Window* window)
	{
		VkResult res;
		res = volkInitialize();
		if (res == VK_ERROR_INITIALIZATION_FAILED) {
			CRITICAL("Unable to load vulkan, is it installed?");
			throw std::runtime_error("Unable to load vulkan, is it installed?");
		}
		assert(res == VK_SUCCESS);

		pimpl = std::make_unique<Impl>(appInfo, window);


	}

	Device::~Device()
	{
	}

}

#endif
