#pragma once

#include <tuple>
#include <vector>

#include <SDL2/SDL_vulkan.h>

#include <volk.h>

#include <vk_mem_alloc.h>

#include "gfx.h"

namespace engine {

	struct DepthStencil {
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
	};

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE; // the associated device
		VmaAllocator allocator = VK_NULL_HANDLE; // the associated allocator
		VkSurfaceFormatKHR surfaceFormat{};
		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		VkPresentModeKHR presentMode{};
		VkExtent2D extent{};
		//VkRenderPass renderpass = VK_NULL_HANDLE;
		VkFormat depthStencilFormat;
		std::vector<std::pair<VkImage, VkImageView>> swapchainImages{};
		std::vector<DepthStencil> depthImages{};
		//std::vector<VkFramebuffer> framebuffers{};
	};

	struct SwapchainInfo {
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkSurfaceKHR surface;
		SDL_Window* window;
		VmaAllocator allocator;
		gfx::PresentMode requested_present_mode;
	};

	void createSwapchain(Swapchain* sc, const SwapchainInfo& info);
	void destroySwapchain(const Swapchain& sc);

}
