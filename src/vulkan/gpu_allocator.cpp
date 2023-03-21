#include <assert.h>

#include <volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "gpu_allocator.h"

namespace engine {

	VmaAllocator createAllocator(VkInstance instance, const Device& device)
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
			.physicalDevice = device.physicalDevice,
			.device = device.device,
			.preferredLargeHeapBlockSize = 0,
			.pAllocationCallbacks = nullptr,
			.pDeviceMemoryCallbacks = nullptr,
			.pHeapSizeLimit = nullptr,
			.pVulkanFunctions = &functions,
			.instance = instance,
			.vulkanApiVersion = VK_API_VERSION_1_3,
			.pTypeExternalMemoryHandleTypes = nullptr
		};

		if (std::find(device.enabledExtensions.begin(), device.enabledExtensions.end(),
			std::string(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) != device.enabledExtensions.end()) {
				createInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
		}
		if (std::find(device.enabledExtensions.begin(), device.enabledExtensions.end(),
			std::string(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) != device.enabledExtensions.end()) {
				createInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		}

		[[maybe_unused]] VkResult res;
		VmaAllocator allocator;
		res = vmaCreateAllocator(&createInfo, &allocator);
		assert(res == VK_SUCCESS);

		return allocator;

	}

	void destroyAllocator(VmaAllocator allocator)
	{
		vmaDestroyAllocator(allocator);
	}

}
