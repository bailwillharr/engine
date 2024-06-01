#pragma once
#include <vk_mem_alloc.h>

#include "device.h"

namespace engine {

	VmaAllocator createAllocator(VkInstance instance, const Device& device);
	void destroyAllocator(VmaAllocator allocator);

}
