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
#include <unordered_set>

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
			// release mode; don't use validation layer
			m_layerInfo = std::make_unique<LayerInfo>(false);
#else
			// debug mode; use validation layer
			m_layerInfo = std::make_unique<LayerInfo>(true);
#endif
			m_instance = std::make_shared<Instance>(appInfo, *m_layerInfo, getRequiredVulkanExtensions(window));
			m_debugMessenger = std::make_unique<DebugMessenger>(m_instance);

			// enumerate physical devices
			uint32_t physDeviceCount = 0;
			VkResult res;
			res = vkEnumeratePhysicalDevices(m_instance->getHandle(), &physDeviceCount, nullptr);
			assert(res == VK_SUCCESS);
			if (physDeviceCount == 0) {
				throw std::runtime_error("No GPU found with vulkan support!");
			}
			std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
			res = vkEnumeratePhysicalDevices(m_instance->getHandle(), &physDeviceCount, physicalDevices.data());
			assert(res == VK_SUCCESS);

			// find suitable device
			const std::vector<const char*> requiredDeviceExtensions{
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};

			VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

			for (const auto& dev : physicalDevices) {

				uint32_t extensionCount;
				res = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
				assert(res == VK_SUCCESS);
				std::vector<VkExtensionProperties> availableExtensions(extensionCount);
				res = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, availableExtensions.data());
				assert(res == VK_SUCCESS);

				bool suitable = true;

				for (const auto& extToFind : requiredDeviceExtensions) {

					bool extFound = false;

					for (const auto& ext : availableExtensions) {
						if (strcmp(extToFind, ext.extensionName) == 0) {
							extFound = true;
						}
					}

					if (!extFound) {
						suitable = false;
					}
				}

				if (suitable) {
					physicalDevice = dev;
					break;
				}

			}

			if (physicalDevice == VK_NULL_HANDLE) {
				throw std::runtime_error("No suitable Vulkan physical device found");
			}

			VkPhysicalDeviceProperties devProps;
			vkGetPhysicalDeviceProperties(physicalDevice, &devProps);
			TRACE("Physical device to use: {}", devProps.deviceName);

			// queue families

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

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
			if (graphicsFamilyIndex.has_value() == false || transferFamilyIndex.has_value() == false) {
				throw std::runtime_error("Unable to find a queue with the GRAPHICS family flag");
			}

			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

			// use a set to filter out duplicate indices
			std::unordered_set<uint32_t> uniqueQueueFamilies{ graphicsFamilyIndex.value(), transferFamilyIndex.value(), computeFamilyIndex.value() };
			float queuePriority = 1.0f;
			for (uint32_t family : uniqueQueueFamilies) {
				// create a queue for each unique type to ensure that there are queues available for graphics, transfer, and compute
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

			VkDevice device;

			res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("Unable to create Vulkan logical device, error code: " + std::to_string(res));
			}

			volkLoadDevice(device);

			vkDestroyDevice(device, nullptr);

		}

	private:

//		VkSurfaceKHR m_surface;

		struct LayerInfo {

			LayerInfo(bool useValidation)
			{
				VkResult res;

				uint32_t layerCount;
				res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
				assert(res == VK_SUCCESS);
				layersAvailable.resize(layerCount);
				res = vkEnumerateInstanceLayerProperties(&layerCount, layersAvailable.data());
				assert(res == VK_SUCCESS);

				if (useValidation == true) {
					// find validation layer and print all layers to log
					for (auto it = layersAvailable.begin(); it != layersAvailable.end(); it++) {
						if (strncmp(it->layerName, LayerInfo::VALIDATION_LAYER_NAME, 256) == 0) {
							validationLayer = it;
						}
					}
					if (validationLayer.has_value() == false) {
						throw std::runtime_error("The validation layer was not found. Quitting.");
					}
				}
			}

			static constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

			std::vector<VkLayerProperties> layersAvailable{};
			std::optional<std::vector<VkLayerProperties>::iterator> validationLayer;

		};

		class Instance {

		public:
			Instance(AppInfo appInfo, const LayerInfo& layerInfo, const std::vector<const char*>& windowExtensions)
			{
				VkResult res;

				int appVersionMajor = 0, appVersionMinor = 0, appVersionPatch = 0;
				assert(versionFromCharArray(appInfo.version, &appVersionMajor, &appVersionMinor, &appVersionPatch));
				int engineVersionMajor = 0, engineVersionMinor = 0, engineVersionPatch = 0;
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

				// also use debug utils extension
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

				std::vector<const char*> layers{};

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
					throw std::runtime_error("The graphics driver is incompatible with vulkan");
				}
				assert(res == VK_SUCCESS);

				volkLoadInstanceOnly(m_handle);

			}

			Instance(const Instance&) = delete;
			Instance& operator=(const Instance&) = delete;
			~Instance()
			{
				vkDestroyInstance(m_handle, nullptr);
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

			DebugMessenger(const DebugMessenger&) = delete;
			DebugMessenger& operator=(const DebugMessenger&) = delete;
			~DebugMessenger()
			{
				vkDestroyDebugUtilsMessengerEXT(m_instance->getHandle(), m_messengerHandle, nullptr);
			}

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
				case Severity::VERBOSE:
					createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
					// fall-through
				case Severity::INFO:
					createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
					// fall-through
				case Severity::WARNING:
					createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
					// fall-through
				case Severity::ERROR:
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
			
			enum class Severity {
				VERBOSE,
				INFO,
				WARNING,
				ERROR
			};
			static constexpr Severity MESSAGE_LEVEL = Severity::WARNING;

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

		std::unique_ptr<LayerInfo> m_layerInfo;
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