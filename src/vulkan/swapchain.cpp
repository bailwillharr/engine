#include <stdexcept>
#include <vector>
#include <algorithm>
#include <assert.h>

#include <SDL2/SDL_vulkan.h>

#include "log.hpp"

#include "swapchain.h"

namespace engine {

	void createSwapchain(Swapchain* sc, const SwapchainInfo& info)
	{
		sc->device = info.device;
		sc->allocator = info.allocator;

		LOG_INFO("Recreating swapchain!\n");

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

		// check there is at least one supported present mode
		uint32_t surfacePresentModeCount = 0;
		std::vector<VkPresentModeKHR> presentModes{};
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(info.physicalDevice, info.surface, &surfacePresentModeCount, nullptr);
		assert(res == VK_SUCCESS);
		presentModes.resize(surfacePresentModeCount);
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(info.physicalDevice, info.surface, &surfacePresentModeCount, presentModes.data());
		assert(res == VK_SUCCESS);

		sc->presentMode = VK_PRESENT_MODE_FIFO_KHR; // This mode is always available
		if (info.vsync == true) {
			if (info.waitForPresent == false) {
				for (const auto& presMode : presentModes) {
					if (presMode == VK_PRESENT_MODE_MAILBOX_KHR) {
						sc->presentMode = presMode; // this mode allows V-sync without fixing FPS to refresh rate
					}
				}
			}
		}
		else {
			for (const auto& presMode : presentModes) {
				if (presMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
					sc->presentMode = presMode; // V-sync off
				}
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

		/* TODO: delete old framebuffers and image views */

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

		{ /* create the depth buffer */

			sc->depthStencil.format = VK_FORMAT_D32_SFLOAT;

			if (sc->depthStencil.image != VK_NULL_HANDLE) {
				vkDestroyImageView(info.device, sc->depthStencil.view, nullptr);
				vmaDestroyImage(sc->allocator, sc->depthStencil.image, sc->depthStencil.allocation);
			}
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.pNext = nullptr;
			imageInfo.flags = 0;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = sc->depthStencil.format;
			imageInfo.extent.width = sc->extent.width;
			imageInfo.extent.height = sc->extent.height;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.queueFamilyIndexCount = 0; // ignored
			imageInfo.pQueueFamilyIndices = nullptr; // ignored
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VmaAllocationCreateInfo allocInfo{};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocInfo.priority = 1.0f;

			res = vmaCreateImage(sc->allocator, &imageInfo, &allocInfo, &sc->depthStencil.image, &sc->depthStencil.allocation, nullptr);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create depth buffer image! Code: " + std::to_string(res));

			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.pNext = nullptr;
			viewInfo.flags = 0;
			viewInfo.image = sc->depthStencil.image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = sc->depthStencil.format;
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			res = vkCreateImageView(info.device, &viewInfo, nullptr, &sc->depthStencil.view);
			assert(res == VK_SUCCESS);

		}

		/* create the render pass */
		if (sc->renderpass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(sc->device, sc->renderpass, nullptr);
		}
		VkAttachmentDescription colorAttachment{
			.flags = 0,
			.format = sc->surfaceFormat.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // ignored
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // ignored
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};
		VkAttachmentDescription depthStencilAttachment{
			.flags = 0,
			.format = sc->depthStencil.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // the depth buffer is not used after the fragment shader
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // ignored
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // ignored
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};
		std::array<VkAttachmentDescription, 2> attachments { colorAttachment, depthStencilAttachment };
		VkAttachmentReference colorAttachmentRef{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};
		VkAttachmentReference depthStencilAttachmentRef{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		};
		VkSubpassDescription subpass{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentRef,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = &depthStencilAttachmentRef,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = nullptr,
		};
		VkSubpassDependency dependency{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0
		};
		VkRenderPassCreateInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.attachmentCount = attachments.size(),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 1,
			.pDependencies = &dependency,
		};
		res = vkCreateRenderPass(sc->device, &renderPassInfo, nullptr, &sc->renderpass);
		if (res != EXIT_SUCCESS) throw std::runtime_error("Failed to create renderpass!");

		// get all the image handles
		uint32_t swapchainImageCount = 0;
		res = vkGetSwapchainImagesKHR(info.device, sc->swapchain, &swapchainImageCount, nullptr);
		assert(res == VK_SUCCESS);
		std::vector<VkImage> swapchainImages(swapchainImageCount);
		res = vkGetSwapchainImagesKHR(info.device, sc->swapchain, &swapchainImageCount, swapchainImages.data());
		assert(res == VK_SUCCESS);

		/* create image view and framebuffer for each image */
		sc->images.resize(swapchainImageCount);
		for (uint32_t i = 0; i < swapchainImageCount; i++) {
			auto& [image, imageView, framebuffer] = sc->images.at(i);

			if (imageView != VK_NULL_HANDLE) vkDestroyImageView(sc->device, imageView, nullptr);
			if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(sc->device, framebuffer, nullptr);

			image = swapchainImages[i];

			/* make the image view */
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.pNext = nullptr;
			viewInfo.flags = 0;
			viewInfo.image = image;
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
			VkResult res = vkCreateImageView(sc->device, &viewInfo, nullptr, &imageView);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create image view from swapchain image!");

			std::array<VkImageView, 2> attachments {
				imageView, sc->depthStencil.view
			};

			VkFramebufferCreateInfo fbInfo{};
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.pNext = nullptr;
			fbInfo.flags = 0;
			fbInfo.renderPass = sc->renderpass;
			fbInfo.attachmentCount = attachments.size();
			fbInfo.pAttachments = attachments.data();
			fbInfo.width = sc->extent.width;
			fbInfo.height = sc->extent.height;
			fbInfo.layers = 1;

			res = vkCreateFramebuffer(sc->device, &fbInfo, nullptr, &framebuffer);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create framebuffer for swapchain image!");

		}

	}

	void destroySwapchain(const Swapchain& sc)
	{
		for (const auto& [image, view, framebuffer] : sc.images) {
			vkDestroyFramebuffer(sc.device, framebuffer, nullptr);
			vkDestroyImageView(sc.device, view, nullptr);
		}
		vkDestroyRenderPass(sc.device, sc.renderpass, nullptr);
		vkDestroyImageView(sc.device, sc.depthStencil.view, nullptr);
		vmaDestroyImage(sc.allocator, sc.depthStencil.image, sc.depthStencil.allocation);
		vkDestroySwapchainKHR(sc.device, sc.swapchain, nullptr);
	}

}
