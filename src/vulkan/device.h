#pragma once

#include <vector>

#include <Volk/volk.h>

namespace engine {

	struct DeviceRequirements {
		std::vector<const char*> requiredExtensions;
		VkPhysicalDeviceFeatures requiredFeatures;
		bool sampledImageLinearFilter;
	};

	struct Device {
		VkDevice device = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties properties{};
		VkPhysicalDeviceFeatures features{};

		struct DeviceQueues {
			VkQueue presentQueue = VK_NULL_HANDLE;
			std::vector<VkQueue> drawQueues{};
			std::vector<VkQueue> transferQueues{};

			uint32_t presentAndDrawQueueFamily = UINT32_MAX;
			uint32_t transferQueueFamily = UINT32_MAX;
		} queues{};

		struct CommandPools {
			VkCommandPool draw = VK_NULL_HANDLE; // use with the drawQueues
			VkCommandPool transfer = VK_NULL_HANDLE; // use with the transferQueues
		} commandPools{};
	};

	Device createDevice(VkInstance instance, DeviceRequirements requirements, VkSurfaceKHR surface);
	void destroyDevice(Device device);

}