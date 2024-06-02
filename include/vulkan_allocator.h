#pragma once

#include <volk.h>

#include <vk_mem_alloc.h>

#include "vulkan_device.h"

namespace engine {

	VmaAllocator createAllocator(VkInstance instance, const Device& device);
	void destroyAllocator(VmaAllocator allocator);

}
