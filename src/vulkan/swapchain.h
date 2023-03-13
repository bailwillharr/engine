#pragma once

#include <tuple>

#include <SDL2/SDL_vulkan.h>

#include <volk.h>

namespace engine {

	struct Swapchain {
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE; // the associated device
		VkSurfaceFormatKHR surfaceFormat{};
		VkSurfaceCapabilitiesKHR surfaceCapabilities{};
		VkPresentModeKHR presentMode{};
		VkExtent2D extent{};
		VkRenderPass renderpass = VK_NULL_HANDLE;
		std::vector<std::tuple<VkImage, VkImageView, VkFramebuffer>> images{};
	};

	struct SwapchainInfo {
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkSurfaceKHR surface;
		SDL_Window* window;
		bool vsync;
		bool waitForPresent;
	};

	void createSwapchain(Swapchain* sc, const SwapchainInfo& info);
	void destroySwapchain(const Swapchain& sc);

}
