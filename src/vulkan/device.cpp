#include <stdexcept>
#include <vector>
#include <array>
#include <string>
#include <cstring>
#include <assert.h>

#include "device.h"

namespace engine {

	/* chooses a device, creates it, gets its function pointers, and creates command pools */
	Device createDevice(VkInstance instance, DeviceRequirements requirements, VkSurfaceKHR surface)
	{
		Device d{};

		// enumerate physical devices
		uint32_t physDeviceCount = 0;
		VkResult res;
		res = vkEnumeratePhysicalDevices(instance, &physDeviceCount, nullptr);
		assert(res == VK_SUCCESS);
		if (physDeviceCount == 0) {
			throw std::runtime_error("No GPU found with vulkan support!");
		}
		std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
		res = vkEnumeratePhysicalDevices(instance, &physDeviceCount, physicalDevices.data());
		assert(res == VK_SUCCESS);

		for (VkPhysicalDevice physDev : physicalDevices) {

			// first, check extension support
			uint32_t extensionCount;
			res = vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extensionCount, nullptr);
			assert(res == VK_SUCCESS);
			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			res = vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extensionCount, availableExtensions.data());
			assert(res == VK_SUCCESS);

			bool foundRequiredExtensions = true;
			for (const char* extToFind : requirements.requiredExtensions) {
				bool extFound = false;
				for (const auto& ext : availableExtensions) {
					if (strcmp(extToFind, ext.extensionName) == 0) {
						extFound = true;
						break;
					}
				}
				if (!extFound) {
					foundRequiredExtensions = false;
					break;
				}
			}
			if (!foundRequiredExtensions) continue; // NEXT!

			VkPhysicalDeviceProperties devProps;
			vkGetPhysicalDeviceProperties(physDev, &devProps);

			// check that the device supports vulkan 1.3
			if (devProps.apiVersion < VK_API_VERSION_1_3) {
				continue;
			}

			/* check features */
			VkPhysicalDeviceFeatures devFeatures;
			vkGetPhysicalDeviceFeatures(physDev, &devFeatures);
			{
				if (requirements.requiredFeatures.robustBufferAccess)
					if (devFeatures.robustBufferAccess == VK_FALSE) continue;
				if (requirements.requiredFeatures.fullDrawIndexUint32)
					if (devFeatures.fullDrawIndexUint32 == VK_FALSE) continue;
				if (requirements.requiredFeatures.imageCubeArray == VK_TRUE)
					if (devFeatures.imageCubeArray == VK_FALSE) continue;
				if (requirements.requiredFeatures.independentBlend == VK_TRUE)
					if (devFeatures.independentBlend == VK_FALSE) continue;
				if (requirements.requiredFeatures.geometryShader == VK_TRUE)
					if (devFeatures.geometryShader == VK_FALSE) continue;
				if (requirements.requiredFeatures.tessellationShader == VK_TRUE)
					if (devFeatures.tessellationShader == VK_FALSE) continue;
				if (requirements.requiredFeatures.sampleRateShading == VK_TRUE)
					if (devFeatures.sampleRateShading == VK_FALSE) continue;
				if (requirements.requiredFeatures.dualSrcBlend == VK_TRUE)
					if (devFeatures.dualSrcBlend == VK_FALSE) continue;
				if (requirements.requiredFeatures.logicOp == VK_TRUE)
					if (devFeatures.logicOp == VK_FALSE) continue;
				if (requirements.requiredFeatures.multiDrawIndirect == VK_TRUE)
					if (devFeatures.multiDrawIndirect == VK_FALSE) continue;
				if (requirements.requiredFeatures.drawIndirectFirstInstance == VK_TRUE)
					if (devFeatures.drawIndirectFirstInstance == VK_FALSE) continue;
				if (requirements.requiredFeatures.depthClamp == VK_TRUE)
					if (devFeatures.depthClamp == VK_FALSE) continue;
				if (requirements.requiredFeatures.depthBiasClamp == VK_TRUE)
					if (devFeatures.depthBiasClamp == VK_FALSE) continue;
				if (requirements.requiredFeatures.fillModeNonSolid == VK_TRUE)
					if (devFeatures.fillModeNonSolid == VK_FALSE) continue;
				if (requirements.requiredFeatures.depthBounds == VK_TRUE)
					if (devFeatures.depthBounds == VK_FALSE) continue;
				if (requirements.requiredFeatures.wideLines == VK_TRUE)
					if (devFeatures.wideLines == VK_FALSE) continue;
				if (requirements.requiredFeatures.largePoints == VK_TRUE)
					if (devFeatures.largePoints == VK_FALSE) continue;
				if (requirements.requiredFeatures.alphaToOne == VK_TRUE)
					if (devFeatures.alphaToOne == VK_FALSE) continue;
				if (requirements.requiredFeatures.multiViewport == VK_TRUE)
					if (devFeatures.multiViewport == VK_FALSE) continue;
				if (requirements.requiredFeatures.samplerAnisotropy == VK_TRUE)
					if (devFeatures.samplerAnisotropy == VK_FALSE) continue;
				if (requirements.requiredFeatures.textureCompressionETC2 == VK_TRUE)
					if (devFeatures.textureCompressionETC2 == VK_FALSE) continue;
				if (requirements.requiredFeatures.textureCompressionASTC_LDR == VK_TRUE)
					if (devFeatures.textureCompressionASTC_LDR == VK_FALSE) continue;
				if (requirements.requiredFeatures.textureCompressionBC == VK_TRUE)
					if (devFeatures.textureCompressionBC == VK_FALSE) continue;
				if (requirements.requiredFeatures.occlusionQueryPrecise == VK_TRUE)
					if (devFeatures.occlusionQueryPrecise == VK_FALSE) continue;
				if (requirements.requiredFeatures.pipelineStatisticsQuery == VK_TRUE)
					if (devFeatures.pipelineStatisticsQuery == VK_FALSE) continue;
				if (requirements.requiredFeatures.vertexPipelineStoresAndAtomics == VK_TRUE)
					if (devFeatures.vertexPipelineStoresAndAtomics == VK_FALSE) continue;
				if (requirements.requiredFeatures.fragmentStoresAndAtomics == VK_TRUE)
					if (devFeatures.fragmentStoresAndAtomics == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderTessellationAndGeometryPointSize == VK_TRUE)
					if (devFeatures.shaderTessellationAndGeometryPointSize == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderImageGatherExtended == VK_TRUE)
					if (devFeatures.shaderImageGatherExtended == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageExtendedFormats == VK_TRUE)
					if (devFeatures.shaderStorageImageExtendedFormats == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageMultisample == VK_TRUE)
					if (devFeatures.shaderStorageImageMultisample == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageReadWithoutFormat == VK_TRUE)
					if (devFeatures.shaderStorageImageReadWithoutFormat == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageWriteWithoutFormat == VK_TRUE)
					if (devFeatures.shaderStorageImageWriteWithoutFormat == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderUniformBufferArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.shaderUniformBufferArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderSampledImageArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.shaderSampledImageArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageBufferArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.shaderStorageBufferArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.shaderStorageImageArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderClipDistance == VK_TRUE)
					if (devFeatures.shaderClipDistance == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderCullDistance == VK_TRUE)
					if (devFeatures.shaderCullDistance == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderFloat64 == VK_TRUE)
					if (devFeatures.shaderFloat64 == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderInt64 == VK_TRUE)
					if (devFeatures.shaderInt64 == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderInt16 == VK_TRUE)
					if (devFeatures.shaderInt16 == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderResourceResidency == VK_TRUE)
					if (devFeatures.shaderResourceResidency == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderResourceMinLod == VK_TRUE)
					if (devFeatures.shaderResourceMinLod == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseBinding == VK_TRUE)
					if (devFeatures.sparseBinding == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyBuffer == VK_TRUE)
					if (devFeatures.sparseResidencyBuffer == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyImage2D == VK_TRUE)
					if (devFeatures.sparseResidencyImage2D == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyImage3D == VK_TRUE)
					if (devFeatures.sparseResidencyImage3D == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency2Samples == VK_TRUE)
					if (devFeatures.sparseResidency2Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency4Samples == VK_TRUE)
					if (devFeatures.sparseResidency4Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency8Samples == VK_TRUE)
					if (devFeatures.sparseResidency8Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency16Samples == VK_TRUE)
					if (devFeatures.sparseResidency16Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyAliased == VK_TRUE)
					if (devFeatures.sparseResidencyAliased == VK_FALSE) continue;
				if (requirements.requiredFeatures.variableMultisampleRate == VK_TRUE)
					if (devFeatures.variableMultisampleRate == VK_FALSE) continue;
				if (requirements.requiredFeatures.inheritedQueries == VK_TRUE)
					if (devFeatures.inheritedQueries == VK_FALSE) continue;
			}

			if (requirements.sampledImageLinearFilter == true) {
				// check for linear filtering for mipmaps
				VkFormatProperties formatProperties{};
				vkGetPhysicalDeviceFormatProperties(physDev, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);
				if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
					continue;
				}
			}

			/* USE THIS PHYSICAL DEVICE */
			d.physicalDevice = physDev;
			d.properties = devProps;
			d.features = requirements.requiredFeatures; // to be safe, only advertise requested features
			//deviceInfo->features = devFeatures;

			break;
		}

		if (d.physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("No suitable Vulkan physical device found");
		}

		/* queue families */
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(d.physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(d.physicalDevice, &queueFamilyCount, queueFamilies.data());

		// find a graphics/present family
		uint32_t graphicsFamily = UINT32_MAX;
		for (size_t i = 0; i < queueFamilies.size(); i++) {
			VkQueueFamilyProperties p = queueFamilies[i];

			if (p.queueCount < 2) continue; // need one queue for presenting and one-or-more for rendering

			if (p.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				VkBool32 supportsPresent;
				res = vkGetPhysicalDeviceSurfaceSupportKHR(d.physicalDevice, static_cast<uint32_t>(i), surface, &supportsPresent);
				if (res != VK_SUCCESS) throw std::runtime_error("Failed to check for queue family present support!");
				if (supportsPresent) {
					graphicsFamily = static_cast<uint32_t>(i);
					break;
				}
			}
		}
		if (graphicsFamily == UINT32_MAX) throw std::runtime_error("Unable to find a graphics/present queue family!");

		// find a transfer queue family (image layout transitions, buffer upload)
		uint32_t transferFamily = UINT32_MAX;
		for (size_t i = 0; i < queueFamilies.size(); i++) {
			VkQueueFamilyProperties p = queueFamilies[i];
			if (((p.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) &&
				((p.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) &&
				((p.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
				transferFamily = static_cast<uint32_t>(i);
				break;
			}
		}
		if (transferFamily == UINT32_MAX) throw std::runtime_error("Unable to find a transfer queue family!");

		// queue priorities
		std::vector<float> graphicsQueuePriorities(queueFamilies[graphicsFamily].queueCount);
		std::fill(graphicsQueuePriorities.begin(), graphicsQueuePriorities.end(), 1.0f);
		std::vector<float> transferQueuePriorities(queueFamilies[transferFamily].queueCount);
		std::fill(transferQueuePriorities.begin(), transferQueuePriorities.end(), 1.0f);

		std::array<VkDeviceQueueCreateInfo, 2> queueCreateInfos{
			VkDeviceQueueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.queueFamilyIndex = graphicsFamily,
				.queueCount = queueFamilies[graphicsFamily].queueCount,
				.pQueuePriorities = graphicsQueuePriorities.data(),
			},
			VkDeviceQueueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.queueFamilyIndex = transferFamily,
				.queueCount = queueFamilies[transferFamily].queueCount,
				.pQueuePriorities = transferQueuePriorities.data(),
			}
		};

		/* create device now */
		VkDeviceCreateInfo deviceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledLayerCount = 0, // deprecated and ignored
			.ppEnabledLayerNames = nullptr, // deprecated and ignored
			.enabledExtensionCount = static_cast<uint32_t>(requirements.requiredExtensions.size()),
			.ppEnabledExtensionNames = requirements.requiredExtensions.data(),
			.pEnabledFeatures = &requirements.requiredFeatures
		};

		res = vkCreateDevice(d.physicalDevice, &deviceCreateInfo, nullptr, &d.device);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Unable to create Vulkan logical device, error code: " + std::to_string(res));
		}

		volkLoadDevice(d.device);

		vkGetDeviceQueue(d.device, graphicsFamily, 0, &d.queues.presentQueue);
		d.queues.drawQueues.resize(queueFamilies[graphicsFamily].queueCount - 1);
		for (uint32_t i = 0; i < d.queues.drawQueues.size(); i++) {
			vkGetDeviceQueue(d.device, graphicsFamily, i + 1, &d.queues.drawQueues[i]);
		}
		d.queues.transferQueues.resize(queueFamilies[transferFamily].queueCount);
		for (uint32_t i = 0; i < d.queues.transferQueues.size(); i++) {
			vkGetDeviceQueue(d.device, transferFamily, i, &d.queues.transferQueues[i]);
		}

		d.queues.presentAndDrawQueueFamily = graphicsFamily;
		d.queues.transferQueueFamily = transferFamily;

		/* generate command pools */
		VkCommandPoolCreateInfo poolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0, // set individually after
			.queueFamilyIndex = 0, // set individually after
		};

		// present queue does not need a command pool as it does not use command buffers

		// draw command pools:
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolCreateInfo.queueFamilyIndex = graphicsFamily;
		res = vkCreateCommandPool(d.device, &poolCreateInfo, nullptr, &d.commandPools.draw);
		if (res != VK_SUCCESS) throw std::runtime_error("Failed to create command pool");

		// transfer command pools:
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // buffers from this pool are often short-lived,
		// as is usually the case for transfer operations
		poolCreateInfo.queueFamilyIndex = transferFamily;
		res = vkCreateCommandPool(d.device, &poolCreateInfo, nullptr, &d.commandPools.transfer);
		if (res != VK_SUCCESS) throw std::runtime_error("Failed to create command pool");

		return d;

	}

	void destroyDevice(Device device)
	{
		vkDestroyCommandPool(device.device, device.commandPools.transfer, nullptr);
		vkDestroyCommandPool(device.device, device.commandPools.draw, nullptr);
		vkDestroyDevice(device.device, nullptr);
	}

}
