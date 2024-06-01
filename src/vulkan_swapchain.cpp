#include <stdexcept>
#include <vector>
#include <array>
#include <algorithm>
#include <assert.h>

#include <SDL2/SDL_vulkan.h>

#include "log.h"

#include "swapchain.h"
#include <SDL_syswm.h>

namespace engine {

	void createSwapchain(Swapchain* sc, const SwapchainInfo& info)
	{
		assert(sc != nullptr);
		sc->device = info.device;
		sc->allocator = info.allocator;

		// get surface caps and features
		VkResult res;
		res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(info.physicalDevice, info.surface, &sc->surfaceCapabilities);
		if (res != VK_SUCCESS) throw std::runtime_error("Unable to get surface capabilities!");

		// check there is at least one supported surface format
		uint32_t surfaceFormatCount = 0;
		std::vector<VkSurfaceFormatKHR> formats{};
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(info.physicalDevice, info.surface, &surfaceFormatCount, nullptr);
		assert(res == VK_SUCCESS);
		formats.resize(surfaceFormatCount);
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(info.physicalDevice, info.surface, &surfaceFormatCount, formats.data());
		assert(res == VK_SUCCESS);

		sc->surfaceFormat = formats[0];
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				sc->surfaceFormat = format; // prefer using srgb non linear colors
			}
		}
		if (sc->surfaceFormat.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			LOG_ERROR("Swapchain not using an SRGB surface!!!");
		}

		// check there is at least one supported present mode
		uint32_t surfacePresentModeCount = 0;
		std::vector<VkPresentModeKHR> presentModes{};
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(info.physicalDevice, info.surface, &surfacePresentModeCount, nullptr);
		assert(res == VK_SUCCESS);
		presentModes.resize(surfacePresentModeCount);
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(info.physicalDevice, info.surface, &surfacePresentModeCount, presentModes.data());
		assert(res == VK_SUCCESS);

		VkPresentModeKHR present_mode_requested;
		switch (info.requested_present_mode) {
		case gfx::PresentMode::kDoubleBufferedNoVsync:
			present_mode_requested = VK_PRESENT_MODE_IMMEDIATE_KHR;
			break;
		case gfx::PresentMode::kDoubleBufferedVsync:
			present_mode_requested = VK_PRESENT_MODE_FIFO_KHR;
			break;
		case gfx::PresentMode::kTripleBuffered:
			present_mode_requested = VK_PRESENT_MODE_MAILBOX_KHR;
		}
		sc->presentMode = VK_PRESENT_MODE_FIFO_KHR; // This mode is always available
		// check requested mode is available
		for (VkPresentModeKHR mode : presentModes) {
			if (mode == present_mode_requested) {
				sc->presentMode = mode;
				break;
			}
		}

		/* get min image count */
		uint32_t minImageCount = sc->surfaceCapabilities.minImageCount + 1;
		if (sc->surfaceCapabilities.maxImageCount > 0 && minImageCount > sc->surfaceCapabilities.maxImageCount) {
			minImageCount = sc->surfaceCapabilities.maxImageCount;
		}

		/* get extent */
		VkExtent2D oldExtent = sc->extent;
		if (sc->surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			sc->extent = sc->surfaceCapabilities.currentExtent;
		}
		else {
			// if fb size isn't already found, get it from SDL
			int width, height;
			SDL_Vulkan_GetDrawableSize(info.window, &width, &height);

			sc->extent.width = static_cast<uint32_t>(width);
			sc->extent.height = static_cast<uint32_t>(height);

			sc->extent.width = std::clamp(
				sc->extent.width,
				sc->surfaceCapabilities.minImageExtent.width, sc->surfaceCapabilities.maxImageExtent.width);
			sc->extent.height = std::clamp(
				sc->extent.height,
				sc->surfaceCapabilities.minImageExtent.height, sc->surfaceCapabilities.maxImageExtent.height);
		}
		if (sc->extent.width == 0 || sc->extent.height == 0) { // preserve extent if the window is minimised
			sc->extent = oldExtent;
		}

		/* find depth stencil format to use */
		VkFormatProperties2 formatProperties{};
		formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		vkGetPhysicalDeviceFormatProperties2(info.physicalDevice, VK_FORMAT_D24_UNORM_S8_UINT, &formatProperties);
		if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			sc->depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		} else {
			vkGetPhysicalDeviceFormatProperties2(info.physicalDevice, VK_FORMAT_D16_UNORM, &formatProperties);
			if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				sc->depthStencilFormat = VK_FORMAT_D16_UNORM;
			} else {
				throw std::runtime_error("Failed to find suitable depth-buffer image format!");
			}
		}

		/* create swapchain */

		VkSwapchainCreateInfoKHR scInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = nullptr,
			.flags = 0,
			.surface = info.surface,
			.minImageCount = minImageCount,
			.imageFormat = sc->surfaceFormat.format,
			.imageColorSpace = sc->surfaceFormat.colorSpace,
			.imageExtent = sc->extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0, // ignored
			.pQueueFamilyIndices = nullptr, // ignored
			.preTransform = sc->surfaceCapabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = sc->presentMode,
			.clipped = VK_TRUE, // discard parts of the screen not visible
			.oldSwapchain = sc->swapchain
		};

		res = vkCreateSwapchainKHR(sc->device, &scInfo, nullptr, &sc->swapchain);
		if (res != VK_SUCCESS) throw std::runtime_error("Failed to create swapchain!");

		/* if it existed, destroy the old swapchain */
		if (scInfo.oldSwapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(info.device, scInfo.oldSwapchain, nullptr);
		}

		// get all the image handles
		uint32_t swapchainImageCount = 0;
		res = vkGetSwapchainImagesKHR(info.device, sc->swapchain, &swapchainImageCount, nullptr);
		assert(res == VK_SUCCESS);
		std::vector<VkImage> swapchainImages(swapchainImageCount);
		res = vkGetSwapchainImagesKHR(info.device, sc->swapchain, &swapchainImageCount, swapchainImages.data());
		assert(res == VK_SUCCESS);

		/* create image view for each image */
		if (sc->swapchainImages.size() == 0) {
			sc->swapchainImages.resize(swapchainImageCount);
			sc->depthImages.resize(swapchainImageCount);
		}
		for (uint32_t i = 0; i < swapchainImageCount; i++) {
			auto& [swapchainImage, swapchainImageView] = sc->swapchainImages.at(i);
			auto& [depthImage, depthAllocation, depthImageView] = sc->depthImages.at(i);

			if (swapchainImageView != VK_NULL_HANDLE) vkDestroyImageView(sc->device, swapchainImageView, nullptr);
			if (depthImageView != VK_NULL_HANDLE) {
				vkDestroyImageView(sc->device, depthImageView, nullptr);
				vmaDestroyImage(sc->allocator, depthImage, depthAllocation);
			}

			swapchainImage = swapchainImages[i];

			/* make the image view */
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.pNext = nullptr;
			viewInfo.flags = 0;
			viewInfo.image = swapchainImage;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = sc->surfaceFormat.format;
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			VkResult res = vkCreateImageView(sc->device, &viewInfo, nullptr, &swapchainImageView);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create image view from swapchain image!");

			/* create the depth buffer */

			VkImageCreateInfo depthImageInfo{};
			depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			depthImageInfo.pNext = nullptr;
			depthImageInfo.flags = 0;
			depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
			depthImageInfo.format = sc->depthStencilFormat;
			depthImageInfo.extent.width = sc->extent.width;
			depthImageInfo.extent.height = sc->extent.height;
			depthImageInfo.extent.depth = 1;
			depthImageInfo.mipLevels = 1;
			depthImageInfo.arrayLayers = 1;
			depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			depthImageInfo.queueFamilyIndexCount = 0; // ignored
			depthImageInfo.pQueueFamilyIndices = nullptr; // ignored
			depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VmaAllocationCreateInfo depthAllocInfo{};
			depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			depthAllocInfo.priority = 1.0f; // this is ignored if VK_EXT_memory_priority isn't found

			// "Consider creating them as dedicated allocations using VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
			//  especially if they are large or if you plan to destroy and recreate them with different sizes
			//  e.g. when display resolution changes."
			// https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
			depthAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

			res = vmaCreateImage(sc->allocator, &depthImageInfo, &depthAllocInfo, &depthImage, &depthAllocation, nullptr);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create depth buffer image! Code: " + std::to_string(res));

			VkImageViewCreateInfo depthViewInfo{};
			depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			depthViewInfo.pNext = nullptr;
			depthViewInfo.flags = 0;
			depthViewInfo.image = depthImage;
			depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			depthViewInfo.format = sc->depthStencilFormat;
			depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			depthViewInfo.subresourceRange.baseMipLevel = 0;
			depthViewInfo.subresourceRange.levelCount = 1;
			depthViewInfo.subresourceRange.baseArrayLayer = 0;
			depthViewInfo.subresourceRange.layerCount = 1;
			res = vkCreateImageView(info.device, &depthViewInfo, nullptr, &depthImageView);
			assert(res == VK_SUCCESS);

		}

		LOG_INFO("Recreating swapchain! w: {} h: {}\n", sc->extent.width, sc->extent.height);

	}

	void destroySwapchain(const Swapchain& sc)
	{
		for (const auto& [image, view] : sc.swapchainImages) {
			vkDestroyImageView(sc.device, view, nullptr);
		}
		for (const auto& [image, allocation, view] : sc.depthImages) {
			vkDestroyImageView(sc.device, view, nullptr);
			vmaDestroyImage(sc.allocator, image, allocation);
		}
		vkDestroySwapchainKHR(sc.device, sc.swapchain, nullptr);
	}

}
