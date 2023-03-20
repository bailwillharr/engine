#include <stdexcept>
#include <vector>
#include <array>
#include <string>
#include <cstring>
#include <assert.h>

#include "log.hpp"

#include "device.h"

namespace engine {

	static bool checkQueueFamilySupportsPresent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex)
	{
		VkBool32 supportsPresent;
		VkResult res;
		res = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, static_cast<uint32_t>(familyIndex), surface, &supportsPresent);
		if (res != VK_SUCCESS) throw std::runtime_error("Failed to check for queue family present support!");
		return supportsPresent;
	}

	/* chooses a device, creates it, gets its function pointers, and retrieves queues */
	Device createDevice(VkInstance instance, const DeviceRequirements& requirements, VkSurfaceKHR surface)
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

		std::vector<VkExtensionProperties> availableExtensions{};

		for (VkPhysicalDevice physDev : physicalDevices) {

			// first, check extension support
			availableExtensions.clear();
			uint32_t extensionCount;
			res = vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extensionCount, nullptr);
			assert(res == VK_SUCCESS);
			availableExtensions.resize(extensionCount);
			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			res = vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extensionCount, availableExtensions.data());
			assert(res == VK_SUCCESS);

			for (const VkExtensionProperties& ext : availableExtensions) {
				LOG_TRACE("extension: {}", ext.extensionName);
			}

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
			VkPhysicalDeviceMemoryPriorityFeaturesEXT memoryPriorityFeatures{};
			memoryPriorityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
			VkPhysicalDeviceFeatures2 devFeatures{};
			devFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			devFeatures.pNext = &memoryPriorityFeatures;
			vkGetPhysicalDeviceFeatures2(physDev, &devFeatures);
			{
				if (requirements.requiredFeatures.robustBufferAccess)
					if (devFeatures.features.robustBufferAccess == VK_FALSE) continue;
				if (requirements.requiredFeatures.fullDrawIndexUint32)
					if (devFeatures.features.fullDrawIndexUint32 == VK_FALSE) continue;
				if (requirements.requiredFeatures.imageCubeArray == VK_TRUE)
					if (devFeatures.features.imageCubeArray == VK_FALSE) continue;
				if (requirements.requiredFeatures.independentBlend == VK_TRUE)
					if (devFeatures.features.independentBlend == VK_FALSE) continue;
				if (requirements.requiredFeatures.geometryShader == VK_TRUE)
					if (devFeatures.features.geometryShader == VK_FALSE) continue;
				if (requirements.requiredFeatures.tessellationShader == VK_TRUE)
					if (devFeatures.features.tessellationShader == VK_FALSE) continue;
				if (requirements.requiredFeatures.sampleRateShading == VK_TRUE)
					if (devFeatures.features.sampleRateShading == VK_FALSE) continue;
				if (requirements.requiredFeatures.dualSrcBlend == VK_TRUE)
					if (devFeatures.features.dualSrcBlend == VK_FALSE) continue;
				if (requirements.requiredFeatures.logicOp == VK_TRUE)
					if (devFeatures.features.logicOp == VK_FALSE) continue;
				if (requirements.requiredFeatures.multiDrawIndirect == VK_TRUE)
					if (devFeatures.features.multiDrawIndirect == VK_FALSE) continue;
				if (requirements.requiredFeatures.drawIndirectFirstInstance == VK_TRUE)
					if (devFeatures.features.drawIndirectFirstInstance == VK_FALSE) continue;
				if (requirements.requiredFeatures.depthClamp == VK_TRUE)
					if (devFeatures.features.depthClamp == VK_FALSE) continue;
				if (requirements.requiredFeatures.depthBiasClamp == VK_TRUE)
					if (devFeatures.features.depthBiasClamp == VK_FALSE) continue;
				if (requirements.requiredFeatures.fillModeNonSolid == VK_TRUE)
					if (devFeatures.features.fillModeNonSolid == VK_FALSE) continue;
				if (requirements.requiredFeatures.depthBounds == VK_TRUE)
					if (devFeatures.features.depthBounds == VK_FALSE) continue;
				if (requirements.requiredFeatures.wideLines == VK_TRUE)
					if (devFeatures.features.wideLines == VK_FALSE) continue;
				if (requirements.requiredFeatures.largePoints == VK_TRUE)
					if (devFeatures.features.largePoints == VK_FALSE) continue;
				if (requirements.requiredFeatures.alphaToOne == VK_TRUE)
					if (devFeatures.features.alphaToOne == VK_FALSE) continue;
				if (requirements.requiredFeatures.multiViewport == VK_TRUE)
					if (devFeatures.features.multiViewport == VK_FALSE) continue;
				if (requirements.requiredFeatures.samplerAnisotropy == VK_TRUE)
					if (devFeatures.features.samplerAnisotropy == VK_FALSE) continue;
				if (requirements.requiredFeatures.textureCompressionETC2 == VK_TRUE)
					if (devFeatures.features.textureCompressionETC2 == VK_FALSE) continue;
				if (requirements.requiredFeatures.textureCompressionASTC_LDR == VK_TRUE)
					if (devFeatures.features.textureCompressionASTC_LDR == VK_FALSE) continue;
				if (requirements.requiredFeatures.textureCompressionBC == VK_TRUE)
					if (devFeatures.features.textureCompressionBC == VK_FALSE) continue;
				if (requirements.requiredFeatures.occlusionQueryPrecise == VK_TRUE)
					if (devFeatures.features.occlusionQueryPrecise == VK_FALSE) continue;
				if (requirements.requiredFeatures.pipelineStatisticsQuery == VK_TRUE)
					if (devFeatures.features.pipelineStatisticsQuery == VK_FALSE) continue;
				if (requirements.requiredFeatures.vertexPipelineStoresAndAtomics == VK_TRUE)
					if (devFeatures.features.vertexPipelineStoresAndAtomics == VK_FALSE) continue;
				if (requirements.requiredFeatures.fragmentStoresAndAtomics == VK_TRUE)
					if (devFeatures.features.fragmentStoresAndAtomics == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderTessellationAndGeometryPointSize == VK_TRUE)
					if (devFeatures.features.shaderTessellationAndGeometryPointSize == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderImageGatherExtended == VK_TRUE)
					if (devFeatures.features.shaderImageGatherExtended == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageExtendedFormats == VK_TRUE)
					if (devFeatures.features.shaderStorageImageExtendedFormats == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageMultisample == VK_TRUE)
					if (devFeatures.features.shaderStorageImageMultisample == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageReadWithoutFormat == VK_TRUE)
					if (devFeatures.features.shaderStorageImageReadWithoutFormat == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageWriteWithoutFormat == VK_TRUE)
					if (devFeatures.features.shaderStorageImageWriteWithoutFormat == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderUniformBufferArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.features.shaderUniformBufferArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderSampledImageArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.features.shaderSampledImageArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageBufferArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.features.shaderStorageBufferArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderStorageImageArrayDynamicIndexing == VK_TRUE)
					if (devFeatures.features.shaderStorageImageArrayDynamicIndexing == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderClipDistance == VK_TRUE)
					if (devFeatures.features.shaderClipDistance == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderCullDistance == VK_TRUE)
					if (devFeatures.features.shaderCullDistance == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderFloat64 == VK_TRUE)
					if (devFeatures.features.shaderFloat64 == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderInt64 == VK_TRUE)
					if (devFeatures.features.shaderInt64 == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderInt16 == VK_TRUE)
					if (devFeatures.features.shaderInt16 == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderResourceResidency == VK_TRUE)
					if (devFeatures.features.shaderResourceResidency == VK_FALSE) continue;
				if (requirements.requiredFeatures.shaderResourceMinLod == VK_TRUE)
					if (devFeatures.features.shaderResourceMinLod == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseBinding == VK_TRUE)
					if (devFeatures.features.sparseBinding == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyBuffer == VK_TRUE)
					if (devFeatures.features.sparseResidencyBuffer == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyImage2D == VK_TRUE)
					if (devFeatures.features.sparseResidencyImage2D == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyImage3D == VK_TRUE)
					if (devFeatures.features.sparseResidencyImage3D == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency2Samples == VK_TRUE)
					if (devFeatures.features.sparseResidency2Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency4Samples == VK_TRUE)
					if (devFeatures.features.sparseResidency4Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency8Samples == VK_TRUE)
					if (devFeatures.features.sparseResidency8Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidency16Samples == VK_TRUE)
					if (devFeatures.features.sparseResidency16Samples == VK_FALSE) continue;
				if (requirements.requiredFeatures.sparseResidencyAliased == VK_TRUE)
					if (devFeatures.features.sparseResidencyAliased == VK_FALSE) continue;
				if (requirements.requiredFeatures.variableMultisampleRate == VK_TRUE)
					if (devFeatures.features.variableMultisampleRate == VK_FALSE) continue;
				if (requirements.requiredFeatures.inheritedQueries == VK_TRUE)
					if (devFeatures.features.inheritedQueries == VK_FALSE) continue;
				
				/* check the memory priority extension was even requested */
				bool memoryPriorityRequired = false;
				for (const char* ext : requirements.requiredExtensions) {
					if (strcmp(ext, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) == 0) {
						memoryPriorityRequired = true;
						break;
					}
				}
				if (memoryPriorityRequired) {
					if (memoryPriorityFeatures.memoryPriority == VK_FALSE) {
						throw std::runtime_error("Required device feature 'memoryPriority' not found, but extension was");
					} else {
						d.memoryPriorityFeature = true;
					}
				} else {
					// see if memoryPriority was optionally requested */
					for (const char* optExt : requirements.optionalExtensions) {
						if (strcmp(optExt, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) == 0) {
							for (const VkExtensionProperties& extAvail : availableExtensions) {
								if (strcmp(extAvail.extensionName, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) == 0) {
									if (memoryPriorityFeatures.memoryPriority == VK_TRUE) {
										d.memoryPriorityFeature = true;
									} else {
										throw std::runtime_error("Optional device extension 'VK_EXT_memory_priority' found, but feature wasn't");
									}
									break;		// |
								}				// |
							}					// V
							// <--------------------
							break;      // |
						}				// |
					}					// V
					// <--------------------
				}
			}

			bool formatsSupported = true;
			for (const FormatRequirements& formatRequirements : requirements.formats) {
				VkFormatFeatureFlags requiredLinearFlags = formatRequirements.properties.linearTilingFeatures;
				VkFormatFeatureFlags requiredOptimalFlags = formatRequirements.properties.optimalTilingFeatures;
				VkFormatFeatureFlags requiredBufferFlags = formatRequirements.properties.bufferFeatures;
				VkFormatProperties deviceFormatProperties{};
				vkGetPhysicalDeviceFormatProperties(physDev, formatRequirements.format, &deviceFormatProperties);
				if ((deviceFormatProperties.linearTilingFeatures & requiredLinearFlags) != requiredLinearFlags ||
					(deviceFormatProperties.optimalTilingFeatures & requiredOptimalFlags) != requiredOptimalFlags ||
					(deviceFormatProperties.bufferFeatures & requiredBufferFlags) != requiredBufferFlags) {
					formatsSupported = false;
					break;
				}
			}
			if (formatsSupported == false) continue;

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

			if (p.queueCount < 2) continue; // ideally have one queue for presenting and at least one other for rendering

			if (p.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				if (checkQueueFamilySupportsPresent(d.physicalDevice, surface, static_cast<uint32_t>(i))) {
					graphicsFamily = static_cast<uint32_t>(i);
					break;
				}
			}
		}
		if (graphicsFamily == UINT32_MAX) {
			for (size_t i = 0; i < queueFamilies.size(); i++) {
				VkQueueFamilyProperties p = queueFamilies[i];
				if (p.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					if (checkQueueFamilySupportsPresent(d.physicalDevice, surface, static_cast<uint32_t>(i))) {
						graphicsFamily = static_cast<uint32_t>(i);
					}
				}
			}
			if (graphicsFamily == UINT32_MAX) {
				throw std::runtime_error("Failed to find a graphics/present family!");
			}
			LOG_WARN("Failed to find ideal graphics/present queue family! Falling back to family #{}.", graphicsFamily);
		}

		// find a transfer queue family (image layout transitions, buffer upload)
		uint32_t transferFamily = UINT32_MAX;
		// prefer a dedicated transfer queue family
		for (size_t i = 0; i < queueFamilies.size(); i++) {
			VkQueueFamilyProperties p = queueFamilies[i];
			if (((p.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) &&
				((p.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) &&
				((p.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
				transferFamily = static_cast<uint32_t>(i);
				break;
			}
		}
		if (transferFamily == UINT32_MAX) {
			transferFamily = graphicsFamily;
			LOG_WARN("Failed to find a dedicated transfer queue family! Falling back to graphics family.");
		}

		// queue priorities
		std::vector<float> graphicsQueuePriorities(queueFamilies[graphicsFamily].queueCount);
		std::fill(graphicsQueuePriorities.begin(), graphicsQueuePriorities.end(), 1.0f);
		std::vector<float> transferQueuePriorities(queueFamilies[transferFamily].queueCount);
		std::fill(transferQueuePriorities.begin(), transferQueuePriorities.end(), 1.0f);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
		queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = graphicsFamily,
			.queueCount = queueFamilies[graphicsFamily].queueCount,
			.pQueuePriorities = graphicsQueuePriorities.data()
		});

		if (transferFamily != graphicsFamily) {
			queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.queueFamilyIndex = transferFamily,
				.queueCount = queueFamilies[transferFamily].queueCount,
				.pQueuePriorities = transferQueuePriorities.data()
			});
		}

		/* set enabled features */
		VkPhysicalDeviceMemoryPriorityFeaturesEXT memoryPriorityFeaturesToEnable{};
		memoryPriorityFeaturesToEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
		memoryPriorityFeaturesToEnable.memoryPriority = d.memoryPriorityFeature ? VK_TRUE : VK_FALSE;
		VkPhysicalDeviceFeatures2 featuresToEnable{};
		featuresToEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		featuresToEnable.pNext = &memoryPriorityFeaturesToEnable;
		featuresToEnable.features = requirements.requiredFeatures;

		/* get list of extensions to enable */
		std::vector<const char*> extensionsToEnable{};
		for (const VkExtensionProperties& availableExt : availableExtensions) {
			if (	std::find(requirements.optionalExtensions.begin(), requirements.optionalExtensions.end(),
					std::string(availableExt.extensionName)) != requirements.optionalExtensions.end()) {
				extensionsToEnable.push_back(availableExt.extensionName);
			}
		}
		extensionsToEnable.insert(extensionsToEnable.end(), requirements.requiredExtensions.begin(), requirements.requiredExtensions.end());

		/* create device now */
		VkDeviceCreateInfo deviceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &featuresToEnable,
			.flags = 0,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledLayerCount = 0, // deprecated and ignored
			.ppEnabledLayerNames = nullptr, // deprecated and ignored
			.enabledExtensionCount = static_cast<uint32_t>(extensionsToEnable.size()),
			.ppEnabledExtensionNames = extensionsToEnable.data(),
			.pEnabledFeatures = nullptr,
		};

		res = vkCreateDevice(d.physicalDevice, &deviceCreateInfo, nullptr, &d.device);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Unable to create Vulkan logical device, error code: " + std::to_string(res));
		}

		volkLoadDevice(d.device);

		/* get list of extensions enabled */
		d.enabledExtensions.clear();
		for (const char* ext : extensionsToEnable) {
			// must be copied into std::strings
			d.enabledExtensions.emplace_back(ext);
		}

		if (transferFamily != graphicsFamily) {
			vkGetDeviceQueue(d.device, graphicsFamily, 0, &d.queues.presentQueue);
			if (queueFamilies[graphicsFamily].queueCount >= 2) {
				d.queues.drawQueues.resize((size_t)queueFamilies[graphicsFamily].queueCount - 1);
				for (uint32_t i = 0; i < d.queues.drawQueues.size(); i++) {
					vkGetDeviceQueue(d.device, graphicsFamily, i + 1, &d.queues.drawQueues[i]);
				}
			} else {
				d.queues.drawQueues.resize(1);
				d.queues.drawQueues[0] = d.queues.presentQueue;
			}
			d.queues.transferQueues.resize(queueFamilies[transferFamily].queueCount);
			for (uint32_t i = 0; i < d.queues.transferQueues.size(); i++) {
				vkGetDeviceQueue(d.device, transferFamily, i, &d.queues.transferQueues[i]);
			}
		} else {
			// same graphics family for graphics/present and transfer
			uint32_t queueCount = queueFamilies[graphicsFamily].queueCount;
			vkGetDeviceQueue(d.device, graphicsFamily, 0, &d.queues.presentQueue);
			if (queueCount >= 2) {
				d.queues.transferQueues.resize(1);
				vkGetDeviceQueue(d.device, graphicsFamily, 1, &d.queues.transferQueues[0]);
				// use the remaining queues for drawing
				if (queueCount >= 3) {
					d.queues.drawQueues.resize((size_t)queueCount - 2);
					for (uint32_t i = 0; i < queueCount - 2; i++) {
						vkGetDeviceQueue(d.device, graphicsFamily, i + 2, &d.queues.drawQueues[i]);
					}
				} else {
					// 2 queues available
					// present and drawing share a queue
					// transfer gets its own queue
					d.queues.drawQueues.resize(1);
					d.queues.drawQueues[0] = d.queues.presentQueue;
				}
			} else {
				// only 1 queue available :(
				d.queues.transferQueues.resize(1);
				d.queues.transferQueues[0] = d.queues.presentQueue;
				d.queues.drawQueues.resize(1);
				d.queues.drawQueues[0] = d.queues.presentQueue;
			}
		}

		d.queues.presentAndDrawQueueFamily = graphicsFamily;
		d.queues.transferQueueFamily = transferFamily;

		return d;

	}

	void destroyDevice(Device device)
	{
		vkDestroyDevice(device.device, nullptr);
	}

}
