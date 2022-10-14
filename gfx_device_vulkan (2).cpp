#if 0

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

#include <array>
#include <iostream>
#include <optional>
#include <unordered_set>

namespace engine {

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

	static VkSurfaceKHR createSurface(SDL_Window* window, VkInstance instance)
	{
		VkSurfaceKHR surface;

		if (SDL_Vulkan_CreateSurface(window, instance, &surface) == false) {
			throw std::runtime_error("Unable to create window surface");
		}

		return surface;
	}

	static uint32_t getMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
			if (typeFilter & (1 << i)) {
				return i;
			}
		}

		throw std::runtime_error("Failed to find a suitable memory type");
	}

	class GFXDevice::Impl {

	public:
		Impl(const char* appName, const char* appVersion, SDL_Window* window)
		{
#ifdef NDEBUG
			// release mode: don't use validation layer
			LayerInfo layerInfo(false);
#else
			// debug mode: use validation layer
			LayerInfo layerInfo(true);
#endif
			auto instance = std::make_shared<Instance>(appName, appVersion, layerInfo, getRequiredVulkanExtensions(window));

			volkLoadInstanceOnly(instance->getHandle());

			m_debugMessenger = std::make_unique<DebugMessenger>(instance); // owns the instance
			auto surface = std::make_shared<Surface>(window, instance); // owns the instance

			auto device = std::make_shared<Device>(surface); // owns the surface
			auto allocator = std::make_shared<GPUAllocator>(device); // owns the device

			m_swapchain = std::make_unique<Swapchain>(device, allocator); // owns the device, allocator

		}

	private:

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
			Instance(const char* appName, const char* appVersion, const LayerInfo& layerInfo, const std::vector<const char*>& windowExtensions)
			{
				VkResult res;

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

			}

			Instance(const Instance&) = delete;
			Instance& operator=(const Instance&) = delete;
			~Instance()
			{
				TRACE("Destroying instance...");
				vkDestroyInstance(m_handle, nullptr);
			}

			VkInstance getHandle() const
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

				VkResult res = vkCreateDebugUtilsMessengerEXT(m_instance->getHandle(), &createInfo, nullptr, &m_messengerHandle);
				assert(res == VK_SUCCESS);
			}

			DebugMessenger(const DebugMessenger&) = delete;
			DebugMessenger& operator=(const DebugMessenger&) = delete;
			~DebugMessenger()
			{
				TRACE("Destroying debug messenger...");
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
			std::shared_ptr<Instance> m_instance;
			VkDebugUtilsMessengerEXT m_messengerHandle;
			
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

		class Surface {

		public:
			Surface(SDL_Window* window, std::shared_ptr<Instance> instance) : m_instance(instance), m_window(window)
			{
				m_handle = createSurface(m_window, instance->getHandle());
			}
			Surface(const Surface&) = delete;
			Surface& operator=(const Surface&) = delete;

			~Surface()
			{
				TRACE("Destroying surface...");
				vkDestroySurfaceKHR(m_instance->getHandle(), m_handle, nullptr);
			}

			VkSurfaceKHR getHandle() const
			{
				return m_handle;
			}

			SDL_Window* getWindow() const
			{
				return m_window;
			}

			VkInstance getInstance() const
			{
				return m_instance->getHandle();
			}

		private:
			std::shared_ptr<Instance> m_instance;
			VkSurfaceKHR m_handle;
			SDL_Window* m_window;

		};

		class Device {

		public:
			Device(std::shared_ptr<Surface> surface) : m_surface(surface)
			{
				// enumerate physical devices
				uint32_t physDeviceCount = 0;
				VkResult res;
				res = vkEnumeratePhysicalDevices(m_surface->getInstance(), &physDeviceCount, nullptr);
				assert(res == VK_SUCCESS);
				if (physDeviceCount == 0) {
					throw std::runtime_error("No GPU found with vulkan support!");
				}
				std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
				res = vkEnumeratePhysicalDevices(m_surface->getInstance(), &physDeviceCount, physicalDevices.data());
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
					res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, m_surface->getHandle(), &m_swapchainSupportDetails.caps);
					assert (res == VK_SUCCESS);

					// check there is at least one supported surface format
					uint32_t surfaceFormatCount = 0;
					res = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface->getHandle(), &surfaceFormatCount, nullptr);
					assert(res == VK_SUCCESS);
					if (surfaceFormatCount == 0) {
						continue;
					}
					m_swapchainSupportDetails.formats.resize(surfaceFormatCount);
					res = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface->getHandle(), &surfaceFormatCount, m_swapchainSupportDetails.formats.data());
					assert(res == VK_SUCCESS);

					// check there is at least one supported present mode
					uint32_t surfacePresentModeCount = 0;
					res = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface->getHandle(), &surfacePresentModeCount, nullptr);
					assert(res == VK_SUCCESS);
					if (surfacePresentModeCount == 0) {
						continue;
					}
					m_swapchainSupportDetails.presentModes.resize(surfacePresentModeCount);
					res = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface->getHandle(), &surfacePresentModeCount, m_swapchainSupportDetails.presentModes.data());
					assert(res == VK_SUCCESS);



					// check physical device properties
					VkPhysicalDeviceProperties devProps;
					vkGetPhysicalDeviceProperties(dev, &devProps);

					// check that the device supports vulkan 1.3
					if (devProps.apiVersion < VK_API_VERSION_1_3) {
						continue;
					}

					m_physicalDevice = dev;
					break;

				} // end for()

				if (m_physicalDevice == VK_NULL_HANDLE) {
					throw std::runtime_error("No suitable Vulkan physical device found");
				}

				VkPhysicalDeviceProperties devProps;
				vkGetPhysicalDeviceProperties(m_physicalDevice, &devProps);
				INFO("Selected physical device: {}", devProps.deviceName);

				TRACE("Supported present modes:");
				for (const auto& presMode : m_swapchainSupportDetails.presentModes) {
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
				vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
				std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

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
				if (	graphicsFamilyIndex.has_value() == false ||
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
					m_queues.push_back(newQueue);
				}

				// check the physical device is compatible with the surface
				VkBool32 graphicsQueueCanPresent;
				res = vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, graphicsFamilyIndex.value(), m_surface->getHandle(), &graphicsQueueCanPresent);
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

				res = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_handle);
				if (res != VK_SUCCESS) {
					throw std::runtime_error("Unable to create Vulkan logical device, error code: " + std::to_string(res));
				}

				volkLoadDevice(m_handle);

				for (auto& q : m_queues) {
					vkGetDeviceQueue(m_handle, q.familyIndex, q.queueIndex, &q.handle);
				}

				Queue gfxQueue = getGraphicsQueue();

				VkCommandPoolCreateInfo gfxCmdPoolInfo{
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
					.queueFamilyIndex = gfxQueue.familyIndex,
				};

				res = vkCreateCommandPool(m_handle, &gfxCmdPoolInfo, nullptr, &m_gfxCommandPool);
				assert(res == VK_SUCCESS);

				VkCommandBufferAllocateInfo gfxCmdBufInfo{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = m_gfxCommandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = 1
				};

				res = vkAllocateCommandBuffers(m_handle, &gfxCmdBufInfo, &m_gfxCommandBuffer);
				assert(res == VK_SUCCESS);

			}
			Device(const Device&) = delete;
			Device& operator=(const Device&) = delete;

			~Device()
			{
				TRACE("Destroying device...");
				vkDestroyCommandPool(m_handle, m_gfxCommandPool, nullptr);
				vkDestroyDevice(m_handle, nullptr);
			}

			VkDevice getHandle() const
			{
				return m_handle;
			}

			VkPhysicalDevice getPhysicalDevice() const
			{
				return m_physicalDevice;
			}

			VkInstance getInstance() const
			{
				return m_surface->getInstance();
			}

			struct SwapchainSupportDetails {
				VkSurfaceCapabilitiesKHR caps{};
				std::vector<VkSurfaceFormatKHR> formats{};
				std::vector<VkPresentModeKHR> presentModes{};
			};

			SwapchainSupportDetails getSupportDetails()
			{
				return m_swapchainSupportDetails;
			}

			struct Queue {
				uint32_t familyIndex;
				uint32_t queueIndex;
				bool supportsGraphics = false;
				bool supportsTransfer = false;
				bool supportsCompute = false;

				VkQueue handle;
			};

			VkCommandBuffer getGraphicsCommandBuffer()
			{
				return m_gfxCommandBuffer;
			}

			std::shared_ptr<Surface> getSurface()
			{
				return m_surface;
			}

			Queue getGraphicsQueue()
			{
				for (const auto& queue : m_queues) {
					if (queue.supportsGraphics) return queue;
				}
				throw std::runtime_error("Unable to find graphics queue");
			}

			Queue getTransferQueue()
			{
				for (const auto& queue : m_queues) {
					if (queue.supportsTransfer) return queue;
				}
				throw std::runtime_error("Unable to find transfer queue");
			}

			/*Queue getComputeQueue()
			{
				for (const auto& queue : m_queues) {
					if (queue.supportsCompute) return queue;
				}
				throw std::runtime_error("Unable to find compute queue");
			}*/

		private:
			std::shared_ptr<Surface> m_surface;

			SwapchainSupportDetails m_swapchainSupportDetails{};

			VkDevice m_handle = VK_NULL_HANDLE;

			VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

			std::vector<Queue> m_queues{};

			VkCommandPool m_gfxCommandPool = VK_NULL_HANDLE;
			VkCommandBuffer m_gfxCommandBuffer = VK_NULL_HANDLE;

		};

		class GPUAllocator {
		public:
			GPUAllocator(std::shared_ptr<const Device> device) : m_device(device)
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
					.physicalDevice = m_device->getPhysicalDevice(),
					.device = m_device->getHandle(),
					.preferredLargeHeapBlockSize = 0,
					.pAllocationCallbacks = nullptr,
					.pDeviceMemoryCallbacks = nullptr,
					.pHeapSizeLimit = nullptr,
					.pVulkanFunctions = &functions,
					.instance = m_device->getInstance(),
					.vulkanApiVersion = VK_API_VERSION_1_3,
					.pTypeExternalMemoryHandleTypes = nullptr
				};

				VkResult res = vmaCreateAllocator(&createInfo, &m_handle);
				assert(res == VK_SUCCESS);

			}
			GPUAllocator(const GPUAllocator&) = delete;
			GPUAllocator& operator=(const GPUAllocator&) = delete;
			~GPUAllocator()
			{
				TRACE("Destroying allocator...");
				vmaDestroyAllocator(m_handle);
			}

			VmaAllocator getHandle() const
			{
				return m_handle;
			}

		private:
			std::shared_ptr<const Device> m_device;
			VmaAllocator m_handle = nullptr;
		};

		class Swapchain {

		public:
			Swapchain(std::shared_ptr<Device> device, std::shared_ptr<GPUAllocator> allocator) : m_device(device), m_allocator(allocator)
			{
				VkResult res;

				auto supportDetails = m_device->getSupportDetails();

				VkSurfaceFormatKHR chosenSurfaceFormat = supportDetails.formats[0];

				for (const auto& format : supportDetails.formats) {
						if (	format.format == VK_FORMAT_B8G8R8A8_SRGB &&
							format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR	) {
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
				} else {
					// if fb size isn't already found, get it from SDL
					int width, height;
					SDL_Vulkan_GetDrawableSize(m_device->getSurface()->getWindow(), &width, &height);

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
					.surface = m_device->getSurface()->getHandle(),
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
					m_device->getGraphicsQueue().familyIndex,
					m_device->getTransferQueue().familyIndex
				};

				// if graphics and transfer queues aren't in the same family
				if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
					createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
					createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
					createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
				}

				res = vkCreateSwapchainKHR(m_device->getHandle(), &createInfo, nullptr, &m_handle);
				assert(res == VK_SUCCESS);

				// get all the image handles
				uint32_t swapchainImageCount = 0;
				res = vkGetSwapchainImagesKHR(m_device->getHandle(), m_handle, &swapchainImageCount, nullptr);
				assert(res == VK_SUCCESS);
				m_images.resize(swapchainImageCount);
				res = vkGetSwapchainImagesKHR(m_device->getHandle(), m_handle, &swapchainImageCount, m_images.data());
				assert(res == VK_SUCCESS);

				m_currentFormat = chosenSurfaceFormat.format;
				m_currentExtent = chosenSwapExtent;

				// create image views
				m_imageViews.clear();
				for (VkImage image : m_images) {

					VkImageViewCreateInfo createInfo{};
					createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					createInfo.pNext = nullptr;
					createInfo.image = image;
					createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					createInfo.format = m_currentFormat;
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
					res = vkCreateImageView(m_device->getHandle(), &createInfo, nullptr, &imageView);
					assert (res == VK_SUCCESS);

					m_imageViews.push_back(imageView);

				}

				// create depth buffer
					
				m_depthBuffer = std::make_unique<DepthStencil>(m_currentExtent, m_device.get(), m_allocator.get());

			}
			Swapchain(const Swapchain&) = delete;
			Swapchain& operator=(const Swapchain&) = delete;

			~Swapchain()
			{
				TRACE("Destroying swapchain...");
				for (VkImageView view : m_imageViews) {
					vkDestroyImageView(m_device->getHandle(), view, nullptr);
				}
				vkDestroySwapchainKHR(m_device->getHandle(), m_handle, nullptr);
			}

		private:
			std::shared_ptr<Device> m_device;
			std::shared_ptr<GPUAllocator> m_allocator;

			VkSwapchainKHR m_handle = VK_NULL_HANDLE;

			std::vector<VkImage> m_images;
			std::vector<VkImageView> m_imageViews;

			VkFormat m_currentFormat{};
			VkExtent2D m_currentExtent{};

			class DepthStencil {
			public:
				DepthStencil(VkExtent2D bufferExtent, Device* device, GPUAllocator* allocator) : m_device(device), m_allocator(allocator)
				{

					// find a suitable format

					const std::vector<VkFormat> formatCandidates{
						VK_FORMAT_D24_UNORM_S8_UINT,
						VK_FORMAT_D32_SFLOAT_S8_UINT,
					};

					const VkFormatFeatureFlags formatFeatures =
						VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

					std::optional<VkFormat> chosenFormat;

					for (VkFormat format : formatCandidates) {
						VkFormatProperties props;
						vkGetPhysicalDeviceFormatProperties(m_device->getPhysicalDevice(), format, &props);

						if ((props.optimalTilingFeatures & formatFeatures) == formatFeatures) {
							chosenFormat = format;
							break;
						}
					}

					if (chosenFormat.has_value() == false) {
						throw std::runtime_error("Unable to find a suitable depth-stencil format");
					}

					m_format = chosenFormat.value();

					switch (m_format) {
					case VK_FORMAT_D24_UNORM_S8_UINT:
						INFO("VK_FORMAT_D24_UNORM_S8_UINT");
						break;
					case VK_FORMAT_D32_SFLOAT_S8_UINT:
						INFO("VK_FORMAT_D32_SFLOAT_S8_UINT");
						break;
					default:
						break;
					}

					VkImageCreateInfo imageCreateInfo{
						.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.imageType = VK_IMAGE_TYPE_2D,
						.format = m_format,
						.extent = {
							.width = bufferExtent.width,
							.height = bufferExtent.height,
							.depth = 1,
						},
						.mipLevels = 1,
						.arrayLayers = 1,
						.samples = VK_SAMPLE_COUNT_1_BIT,
						.tiling = VK_IMAGE_TILING_OPTIMAL,
						.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
						.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
						.queueFamilyIndexCount = 1,
						.pQueueFamilyIndices = nullptr,
						.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					};

					VmaAllocationCreateInfo allocInfo{};
					allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

					VkResult res;

					res = vmaCreateImage(m_allocator->getHandle(), &imageCreateInfo, &allocInfo, &m_image, &m_allocation, nullptr);
					assert(res == VK_SUCCESS);

					VkImageViewCreateInfo createInfo{};
					createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					createInfo.pNext = nullptr;
					createInfo.image = m_image;
					createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					createInfo.format = m_format;
					createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					createInfo.subresourceRange.baseMipLevel = 0;
					createInfo.subresourceRange.levelCount = 1;
					createInfo.subresourceRange.baseArrayLayer = 0;
					createInfo.subresourceRange.layerCount = 1;

					res = vkCreateImageView(m_device->getHandle(), &createInfo, nullptr, &m_imageView);
					assert (res == VK_SUCCESS);
					
				}
				DepthStencil(const DepthStencil&) = delete;
				DepthStencil& operator=(const DepthStencil&) = delete;
				~DepthStencil()
				{
					TRACE("Destroying DepthStencil...");
					vkDestroyImageView(m_device->getHandle(), m_imageView, nullptr);
					// destroy allocation
					vmaDestroyImage(m_allocator->getHandle(), m_image, m_allocation);
				}

			private:

				VkFormat m_format;

				Device* m_device;
				GPUAllocator* m_allocator;

				VkImage m_image = VK_NULL_HANDLE;
				VmaAllocation m_allocation;
				VkImageView m_imageView = VK_NULL_HANDLE;

			};

			std::unique_ptr<DepthStencil> m_depthBuffer;

		};

		std::unique_ptr<DebugMessenger> m_debugMessenger; // uses instance

		std::unique_ptr<Swapchain> m_swapchain;

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		VkResult res;
		res = volkInitialize();
		if (res == VK_ERROR_INITIALIZATION_FAILED) {
			throw std::runtime_error("Unable to load vulkan, is it installed?");
		}
		assert(res == VK_SUCCESS);

		uint32_t vulkanVersion = volkGetInstanceVersion();
		if (vulkanVersion < VK_MAKE_VERSION(1, 3, 0)) {
			throw std::runtime_error("The loaded Vulkan version must be at least 1.3");
		}

		m_pimpl = std::make_unique<Impl>(appName, appVersion, window);

	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");
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

#endif