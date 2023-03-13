#pragma once

#include <tuple>
#include <vector>

#include <SDL2/SDL_vulkan.h>

#include <volk.h>

#include <vk_mem_alloc.h>

namespace engine {

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE; // the associated device
		VmaAllocator allocator = VK_NULL_HANDLE; // the associated allocator
		VkSurfaceFormatKHR surfaceFormat{};
		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		VkPresentModeKHR presentMode{};
		VkExtent2D extent{};
		VkRenderPass renderpass = VK_NULL_HANDLE;
		std::vector<std::tuple<VkImage, VkImageView, VkFramebuffer>> images{};
		struct DepthStencil {
			VkImage image = VK_NULL_HANDLE;
			VmaAllocation allocation = VK_NULL_HANDLE;
			VkImageView view = VK_NULL_HANDLE;
			VkFormat format{};
		} depthStencil{};
	};

	struct SwapchainInfo {
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkSurfaceKHR surface;
		SDL_Window* window;
		VmaAllocator allocator;
		bool vsync;
		bool waitForPresent;
	};

	void createSwapchain(Swapchain* sc, const SwapchainInfo& info);
	void destroySwapchain(const Swapchain& sc);

}
