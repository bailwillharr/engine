#pragma once

#include <vector>
#include <string>

#include <volk.h>

namespace engine {

	struct FormatRequirements {
		VkFormat format{};
		VkFormatProperties properties{};
	};

	struct DeviceRequirements {
		std::vector<const char*> requiredExtensions;
		std::vector<const char*> optionalExtensions;
		VkPhysicalDeviceFeatures requiredFeatures;
		std::vector<FormatRequirements> formats{};
	};

	struct Device {
		VkDevice device = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		std::vector<std::string> enabledExtensions{};
		VkPhysicalDeviceProperties properties{};
		VkPhysicalDeviceFeatures features{};
		bool memoryPriorityFeature = false;
		struct DeviceQueues {
			VkQueue presentQueue = VK_NULL_HANDLE;
			std::vector<VkQueue> drawQueues{};
			std::vector<VkQueue> transferQueues{};

			uint32_t presentAndDrawQueueFamily = UINT32_MAX;
			uint32_t transferQueueFamily = UINT32_MAX;
		} queues{};

	};

	Device createDevice(VkInstance instance, const DeviceRequirements& requirements, VkSurfaceKHR surface);
	void destroyDevice(Device device);

}
