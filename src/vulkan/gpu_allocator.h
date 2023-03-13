#pragma once
#include <vk_mem_alloc.h>

namespace engine {

	VmaAllocator createAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice);
	void destroyAllocator(VmaAllocator allocator);

}
