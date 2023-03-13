// The implementation of the graphics layer using Vulkan 1.3.

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>
#include <deque>
#include <map>
#include <iostream>

#include <SDL_vulkan.h>

#include <shaderc/shaderc.hpp>

#include <volk.h>

#include "gfx_device.hpp"
#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/gpu_allocator.h"
#include "vulkan/swapchain.h"
#include "util.hpp"
#include "config.h"
#include "log.hpp"
#include "util/files.hpp"

inline static void checkVulkanError(VkResult errorCode, int lineNo)
{
	if (errorCode != VK_SUCCESS) {
		const std::string message("VULKAN ERROR ON LINE " + std::to_string(lineNo));
		throw std::runtime_error(message.c_str());
	}
}

#undef VKCHECK
#define VKCHECK(ErrCode) \
        checkVulkanError(ErrCode, __LINE__)

namespace engine {

	static constexpr uint32_t FRAMES_IN_FLIGHT = 2; // This improved FPS by 5x! (on Intel IGPU)

	static constexpr size_t PUSH_CONSTANT_MAX_SIZE = 128; // bytes
	static constexpr VkIndexType INDEX_TYPE = VK_INDEX_TYPE_UINT32;

	// structures and enums

	struct FrameData {
		VkFence renderFence = VK_NULL_HANDLE;
		VkSemaphore presentSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderSemaphore = VK_NULL_HANDLE;
		VkCommandBuffer drawBuf = VK_NULL_HANDLE;
	};

	// handles

	struct gfx::Buffer {
		gfx::BufferType type;
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
		VkDeviceSize size = 0;
		bool hostVisible = false;
	};

	struct gfx::Pipeline {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipeline handle = VK_NULL_HANDLE;
	};

	struct gfx::Texture {
		VkImage image;
		VmaAllocation alloc;
		VkImageView imageView;
		VkSampler sampler;
		VkDescriptorPool pool;
		std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> descriptorSets{};
		uint32_t mipLevels;
	};

	struct gfx::DrawBuffer {
		FrameData frameData;
		uint32_t currentFrameIndex; // corresponds to the frameData
		uint32_t imageIndex; // for swapchain present
	};

	struct gfx::DescriptorSetLayout {
		VkDescriptorSetLayout layout;
	};

	struct gfx::DescriptorSet {
		std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> sets; // frames in flight cannot use the same descriptor set in case the buffer needs updating
	};

	struct gfx::DescriptorBuffer {
		gfx::Buffer stagingBuffer{};
		std::array<gfx::Buffer, FRAMES_IN_FLIGHT> gpuBuffers;
	};

	// enum converters

	namespace vkinternal {

		static VkFormat getVertexAttribFormat(gfx::VertexAttribFormat fmt)
		{
			switch (fmt) {
			case gfx::VertexAttribFormat::FLOAT2:
				return VK_FORMAT_R32G32_SFLOAT;
			case gfx::VertexAttribFormat::FLOAT3:
				return VK_FORMAT_R32G32B32_SFLOAT;
			case gfx::VertexAttribFormat::FLOAT4:
				return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
			throw std::runtime_error("Unknown vertex attribute format");
		}

		static VkBufferUsageFlagBits getBufferUsageFlag(gfx::BufferType type)
		{
			switch (type) {
			case gfx::BufferType::VERTEX:
				return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case gfx::BufferType::INDEX:
				return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			case gfx::BufferType::UNIFORM:
				return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			default:
				throw std::runtime_error("This buffer type does not have usage bits");
			}
		}

		[[maybe_unused]] static VkFilter getTextureFilter(gfx::TextureFilter filter)
		{
			switch (filter) {
			case gfx::TextureFilter::LINEAR:
				return VK_FILTER_LINEAR;
			case gfx::TextureFilter::NEAREST:
				return VK_FILTER_NEAREST;
			}
			throw std::runtime_error("Unknown texture filter");
		}

		[[maybe_unused]] static VkSampleCountFlags getSampleCountFlags(gfx::MSAALevel level)
		{
			switch (level) {
			case gfx::MSAALevel::MSAA_OFF:
				return VK_SAMPLE_COUNT_1_BIT;
				break;
			case gfx::MSAALevel::MSAA_2X:
				return VK_SAMPLE_COUNT_2_BIT;
				break;
			case gfx::MSAALevel::MSAA_4X:
				return VK_SAMPLE_COUNT_4_BIT;
				break;
			case gfx::MSAALevel::MSAA_8X:
				return VK_SAMPLE_COUNT_8_BIT;
				break;
			case gfx::MSAALevel::MSAA_16X:
				return VK_SAMPLE_COUNT_16_BIT;
				break;
			default:
				throw std::runtime_error("Unknown MSAA level");
			}
		}

	}

	// functions

	static VkShaderModule compileShader(VkDevice device, shaderc_shader_kind kind, const std::string& source, const char* filename)
	{

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;

		options.SetSourceLanguage(shaderc_source_language_glsl);
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
		options.SetTargetSpirv(shaderc_spirv_version_1_6);
		options.SetAutoBindUniforms(false);

		// preprocess
		shaderc::PreprocessedSourceCompilationResult preprocessed = compiler.PreprocessGlsl(source, kind, filename, options);

		if (preprocessed.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			throw std::runtime_error("PREPROCESS ERR " + preprocessed.GetErrorMessage());
		}

		std::string shaderStr{ preprocessed.cbegin(), preprocessed.cend() };

		// compile
		shaderc::SpvCompilationResult compiledShader = compiler.CompileGlslToSpv(shaderStr.c_str(), kind, filename, options);

		if (compiledShader.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			throw std::runtime_error("COMPILE ERR " + compiledShader.GetErrorMessage());
		}

		std::vector<uint32_t> shaderBytecode = { compiledShader.cbegin(), compiledShader.cend() };// not sure why sample code copy vector like this

		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderBytecode.size() * sizeof(uint32_t);
		createInfo.pCode = compiledShader.cbegin();

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;

	}

#if 0

	static Swapchain::MSTarget createMSAATarget(VkSampleCountFlagBits msaaSamples, VkExtent2D extent, VkFormat colorFormat, VkDevice device, VmaAllocator allocator)
	{
		Swapchain::MSTarget target{};

		[[maybe_unused]] VkResult res;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = extent.width;
		imageInfo.extent.height = extent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = colorFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = msaaSamples;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocInfo.priority = 1.0f;

		res = vmaCreateImage(allocator, &imageInfo, &allocInfo, &target.colorImage, &target.colorImageAllocation, nullptr);
		assert(res == VK_SUCCESS);

		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = target.colorImage;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = colorFormat;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		res = vkCreateImageView(device, &imageViewInfo, nullptr, &target.colorImageView);
		assert(res == VK_SUCCESS);

		return target;
	}

	static void destroyMSAATarget(const Swapchain::MSTarget& target, VkDevice device, VmaAllocator allocator)
	{
		vkDestroyImageView(device, target.colorImageView, nullptr);
		vmaDestroyImage(allocator, target.colorImage, target.colorImageAllocation);
	}

	static DepthBuffer createDepthBuffer(VkDevice device, VmaAllocator allocator, VkExtent2D extent, VkSampleCountFlagBits msaaSamples)
	{
		DepthBuffer db{};

		[[maybe_unused]] VkResult res;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = extent.width;
		imageInfo.extent.height = extent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_D32_SFLOAT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = msaaSamples;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocInfo.priority = 1.0f;

		res = vmaCreateImage(allocator, &imageInfo, &allocInfo, &db.image, &db.allocation, nullptr);
		assert(res == VK_SUCCESS);

		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = db.image;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_D32_SFLOAT;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;
		res = vkCreateImageView(device, &imageViewInfo, nullptr, &db.view);
		assert(res == VK_SUCCESS);

		return db;
	}

	static void destroyDepthBuffer(DepthBuffer db, VkDevice device, VmaAllocator allocator)
	{
		vkDestroyImageView(device, db.view, nullptr);
		vmaDestroyImage(allocator, db.image, db.allocation);
	}

	static VkSampleCountFlagBits getMaxSampleCount(VkPhysicalDevice physicalDevice, gfx::MSAALevel maxLevel)
	{
		VkSampleCountFlags max = vkinternal::getSampleCountFlags(maxLevel);
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		counts %= (max << 1); // restricts sample count to maxLevel
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
		throw std::runtime_error("MSAA is not supported");
	}
#endif
	static void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		[[maybe_unused]] VkResult res;

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
		assert(res == VK_SUCCESS);

		{ // record the command buffer
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			assert(res == VK_SUCCESS);

			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = size;
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

			res = vkEndCommandBuffer(commandBuffer);
			assert(res == VK_SUCCESS);
		}

		// submit
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(res == VK_SUCCESS);

		res = vkQueueWaitIdle(queue);
		assert(res == VK_SUCCESS);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

	}

#if 0

	static VkCommandBuffer beginOneTimeCommands(VkDevice device, VkCommandPool commandPool)
	{
		[[maybe_unused]] VkResult res;

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		res = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
		assert(res == VK_SUCCESS);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		assert(res == VK_SUCCESS);

		return commandBuffer;
	}

	static void endOneTimeCommands(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue)
	{
		[[maybe_unused]] VkResult res;
		res = vkEndCommandBuffer(commandBuffer);
		assert(res == VK_SUCCESS);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(res == VK_SUCCESS);
		res = vkQueueWaitIdle(queue);
		assert(res == VK_SUCCESS);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	static void cmdTransitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, VkImage image)
	{

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	}

	static void cmdGenerateMipmaps(VkCommandBuffer commandBuffer, VkImage image, int32_t width, int32_t height, uint32_t mipLevels)
	{

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = width;
		int32_t mipHeight = height;
		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;

		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

#endif

	// class definitions

	struct GFXDevice::Impl {

		// device settings
		gfx::GraphicsSettings graphicsSettings;

		SDL_Window* window = nullptr;
		Instance instance{};
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		Device device{};
		VmaAllocator allocator{};
		SwapchainInfo swapchainInfo{};
		Swapchain swapchain{};

		VkDescriptorPool descriptorPool;
		std::array<std::unordered_set<gfx::DescriptorBuffer*>, FRAMES_IN_FLIGHT> descriptorBufferWriteQueues{};

		uint64_t FRAMECOUNT = 0;

		FrameData frameData[FRAMES_IN_FLIGHT] = {};

		bool swapchainIsOutOfDate = false;

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window, gfx::GraphicsSettings settings)
	{

		pimpl = std::make_unique<Impl>();

		VkResult res;

		pimpl->window = window;
		pimpl->graphicsSettings = settings;

		// initialise vulkan

		res = volkInitialize();
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Unable to load vulkan, is it installed?");
		}

		uint32_t vulkanVersion = volkGetInstanceVersion();
		assert(vulkanVersion != 0);
		if (vulkanVersion < VK_API_VERSION_1_3) {
			throw std::runtime_error("The loaded Vulkan version must be at least 1.3");
		}

#ifdef NDEBUG
		bool useValidation = false;
#else
		bool useValidation = true;
#endif

		pimpl->instance = createVulkanInstance(pimpl->window, appName, appVersion, useValidation, MessageSeverity::SEV_WARNING);

		if (SDL_Vulkan_CreateSurface(pimpl->window, pimpl->instance.instance, &pimpl->surface) == false) {
			throw std::runtime_error("Unable to create window surface");
		};

		DeviceRequirements deviceRequirements{};
		deviceRequirements.requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		deviceRequirements.requiredFeatures.samplerAnisotropy = VK_TRUE;
		deviceRequirements.sampledImageLinearFilter = true;
		pimpl->device = createDevice(pimpl->instance.instance, deviceRequirements, pimpl->surface);

		pimpl->allocator = createAllocator(pimpl->instance.instance, pimpl->device.device, pimpl->device.physicalDevice);

		pimpl->swapchainInfo.device = pimpl->device.device;
		pimpl->swapchainInfo.physicalDevice = pimpl->device.physicalDevice;
		pimpl->swapchainInfo.surface = pimpl->surface;
		pimpl->swapchainInfo.window = pimpl->window;
		pimpl->swapchainInfo.vsync = pimpl->graphicsSettings.vsync;
		pimpl->swapchainInfo.waitForPresent = pimpl->graphicsSettings.waitForPresent;
		createSwapchain(&pimpl->swapchain, pimpl->swapchainInfo);

		/* make synchronisation primitives for rendering and allocate command buffers */

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
			};
			res = vkCreateFence(pimpl->device.device, &fenceInfo, nullptr, &pimpl->frameData[i].renderFence);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create fence!");

			VkSemaphoreCreateInfo smphInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0
			};
			res = vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].presentSemaphore);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create semaphore!");
			res = vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].renderSemaphore);
			if (res != VK_SUCCESS) throw std::runtime_error("Failed to create semaphore!");

			VkCommandBufferAllocateInfo cmdAllocInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = pimpl->device.commandPools.draw,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};
			VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &cmdAllocInfo, &pimpl->frameData[i].drawBuf));
		}

		/* create a global descriptor pool */

		std::vector<VkDescriptorPoolSize> poolSizes{};
		poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5); // purposely low limit

		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pNext = nullptr;
		descriptorPoolInfo.flags = 0;
		descriptorPoolInfo.maxSets = 5; // purposely low limit
		descriptorPoolInfo.poolSizeCount = poolSizes.size();
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		VKCHECK(vkCreateDescriptorPool(pimpl->device.device, &descriptorPoolInfo, nullptr, &pimpl->descriptorPool));

	}

	GFXDevice::~GFXDevice()
	{
		vkDestroyDescriptorPool(pimpl->device.device, pimpl->descriptorPool, nullptr);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			vkFreeCommandBuffers(pimpl->device.device, pimpl->device.commandPools.draw, 1, &pimpl->frameData[i].drawBuf);
			vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].renderSemaphore, nullptr);
			vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].presentSemaphore, nullptr);
			vkDestroyFence(pimpl->device.device, pimpl->frameData[i].renderFence, nullptr);
		}

		destroySwapchain(pimpl->swapchain);
		destroyAllocator(pimpl->allocator);
		destroyDevice(pimpl->device);
		vkDestroySurfaceKHR(pimpl->instance.instance, pimpl->surface, nullptr);
		destroyVulkanInstance(pimpl->instance);
	}

	void GFXDevice::getViewportSize(uint32_t* w, uint32_t* h)
	{
		int width, height;
		SDL_Vulkan_GetDrawableSize(pimpl->window, &width, &height);
		if (width == 0 || height == 0) {
			*w = (uint32_t)pimpl->swapchain.extent.width;
			*h = (uint32_t)pimpl->swapchain.extent.height;
		}
		else {
			*w = (uint32_t)width;
			*h = (uint32_t)height;
		}
	}

	gfx::DrawBuffer* GFXDevice::beginRender()
	{
		VkResult res;

		const uint32_t currentFrameIndex = pimpl->FRAMECOUNT % FRAMES_IN_FLIGHT;
		const FrameData frameData = pimpl->frameData[currentFrameIndex];

		/* first empty the descriptor buffer write queue */
		auto& writeQueue = pimpl->descriptorBufferWriteQueues[currentFrameIndex];
		for (gfx::DescriptorBuffer* buffer : writeQueue) {
			copyBuffer(pimpl->device.device, pimpl->device.commandPools.transfer, pimpl->device.queues.transferQueues[0], buffer->stagingBuffer.buffer, buffer->gpuBuffers[currentFrameIndex].buffer, buffer->stagingBuffer.size);
		}
		writeQueue.clear();

		uint32_t swapchainImageIndex;

		do {
			if (pimpl->swapchainIsOutOfDate) {
				// re-create swapchain
				vkQueueWaitIdle(pimpl->device.queues.drawQueues[0]);
				vkQueueWaitIdle(pimpl->device.queues.presentQueue);
				createSwapchain(&pimpl->swapchain, pimpl->swapchainInfo);
			}
			// THIS FUNCTION BLOCKS UNTIL AN IMAGE IS AVAILABLE (it waits for vsync)
			res = vkAcquireNextImageKHR(
				pimpl->device.device, pimpl->swapchain.swapchain, 1000000000LL,
				frameData.presentSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);
			if (res != VK_SUBOPTIMAL_KHR && res != VK_ERROR_OUT_OF_DATE_KHR) VKCHECK(res);
			if (res == VK_SUCCESS) pimpl->swapchainIsOutOfDate = false;
		} while (pimpl->swapchainIsOutOfDate);

		/* wait until the previous frame RENDERING has finished */
		res = vkWaitForFences(pimpl->device.device, 1, &frameData.renderFence, VK_TRUE, 1000000000LL);
		VKCHECK(res);
		res = vkResetFences(pimpl->device.device, 1, &frameData.renderFence);
		VKCHECK(res);

		/* record command buffer */
		res = vkResetCommandBuffer(frameData.drawBuf, 0);
		VKCHECK(res);

		VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr // ignored
		};
		res = vkBeginCommandBuffer(frameData.drawBuf, &beginInfo);
		VKCHECK(res);

		{ // RECORDING

			VkClearValue clearValue{}; // Using same value for all components enables compression according to NVIDIA Best Practices
			clearValue.color.float32[0] = 1.0f;
			clearValue.color.float32[1] = 1.0f;
			clearValue.color.float32[2] = 1.0f;
			clearValue.color.float32[3] = 1.0f;

			VkRenderPassBeginInfo passBegin{};
			passBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passBegin.pNext = nullptr;
			passBegin.renderPass = pimpl->swapchain.renderpass;
			passBegin.framebuffer = std::get<2>(pimpl->swapchain.images[swapchainImageIndex]);
			passBegin.renderArea.extent = pimpl->swapchain.extent;
			passBegin.renderArea.offset = { 0, 0 };
			passBegin.clearValueCount = 1;
			passBegin.pClearValues = &clearValue;
			vkCmdBeginRenderPass(frameData.drawBuf, &passBegin, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = (float)pimpl->swapchain.extent.height;
			viewport.width = (float)pimpl->swapchain.extent.width;
			viewport.height = -(float)pimpl->swapchain.extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(frameData.drawBuf, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = pimpl->swapchain.extent;
			vkCmdSetScissor(frameData.drawBuf, 0, 1, &scissor);

		}

		// hand command buffer over to caller
		gfx::DrawBuffer* drawBuffer = new gfx::DrawBuffer;
		drawBuffer->frameData = frameData;
		drawBuffer->currentFrameIndex = currentFrameIndex;
		drawBuffer->imageIndex = swapchainImageIndex;
		return drawBuffer;

	}

	void GFXDevice::finishRender(gfx::DrawBuffer* drawBuffer)
	{
		if (drawBuffer == nullptr) {
			return;
		}
		uint32_t swapchainImageIndex = drawBuffer->imageIndex;
		VkResult res;

		vkCmdEndRenderPass(drawBuffer->frameData.drawBuf);

		res = vkEndCommandBuffer(drawBuffer->frameData.drawBuf);
		VKCHECK(res);

		// SUBMIT

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &drawBuffer->frameData.presentSemaphore,
			.pWaitDstStageMask = &waitStage,
			.commandBufferCount = 1,
			.pCommandBuffers = &drawBuffer->frameData.drawBuf,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &drawBuffer->frameData.renderSemaphore,
		};
		res = vkQueueSubmit(pimpl->device.queues.drawQueues[0], 1, &submitInfo, drawBuffer->frameData.renderFence);
		// VKCHECK(res); // expensive operation for some reason

		// PRESENT

		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &drawBuffer->frameData.renderSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &pimpl->swapchain.swapchain,
			.pImageIndices = &swapchainImageIndex,
			.pResults = nullptr
		};
		res = vkQueuePresentKHR(pimpl->device.queues.presentQueue, &presentInfo);
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
			// flag to re-create the swapchain before next render
			pimpl->swapchainIsOutOfDate = true;
		}
		else if (res != VK_SUCCESS) throw std::runtime_error("Failed to queue present!");

		pimpl->FRAMECOUNT++;

		delete drawBuffer;
	}

	void GFXDevice::cmdBindPipeline(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline)
	{
		assert(drawBuffer != nullptr);
		vkCmdBindPipeline(drawBuffer->frameData.drawBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
	}

	void GFXDevice::cmdBindVertexBuffer(gfx::DrawBuffer* drawBuffer, uint32_t binding, const gfx::Buffer* buffer)
	{
		assert(drawBuffer != nullptr);
		assert(buffer != nullptr);
		assert(buffer->type == gfx::BufferType::VERTEX);
		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(drawBuffer->frameData.drawBuf, binding, 1, &buffer->buffer, &offset);
	}

	void GFXDevice::cmdBindIndexBuffer(gfx::DrawBuffer* drawBuffer, const gfx::Buffer* buffer)
	{
		assert(drawBuffer != nullptr);
		assert(buffer != nullptr);
		assert(buffer->type == gfx::BufferType::INDEX);
		vkCmdBindIndexBuffer(drawBuffer->frameData.drawBuf, buffer->buffer, 0, INDEX_TYPE);
	}

	void GFXDevice::cmdDrawIndexed(gfx::DrawBuffer* drawBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		assert(drawBuffer != nullptr);
		vkCmdDrawIndexed(drawBuffer->frameData.drawBuf, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void GFXDevice::cmdPushConstants(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline, uint32_t offset, uint32_t size, const void* data)
	{
		assert(drawBuffer != nullptr);
		vkCmdPushConstants(drawBuffer->frameData.drawBuf, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, offset, size, data);
	}

	void GFXDevice::cmdBindDescriptorSet(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline, const gfx::DescriptorSet* set, uint32_t setNumber)
	{
		vkCmdBindDescriptorSets(drawBuffer->frameData.drawBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, setNumber, 1, &set->sets[drawBuffer->currentFrameIndex], 0, nullptr);
	}

	gfx::Pipeline* GFXDevice::createPipeline(const gfx::PipelineInfo& info)
	{

		[[maybe_unused]] VkResult res;

		gfx::Pipeline* pipeline = new gfx::Pipeline;

		auto vertShaderCode = util::readTextFile(info.vertShaderPath);
		auto fragShaderCode = util::readTextFile(info.fragShaderPath);

		VkShaderModule vertShaderModule = compileShader(pimpl->device.device, shaderc_vertex_shader, vertShaderCode->data(), info.vertShaderPath);
		VkShaderModule fragShaderModule = compileShader(pimpl->device.device, shaderc_fragment_shader, fragShaderCode->data(), info.fragShaderPath);

		// get vertex attrib layout:
		VkVertexInputBindingDescription bindingDescription{ };
		bindingDescription.binding = 0;
		bindingDescription.stride = info.vertexFormat.stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attribDescs{};
		attribDescs.reserve(info.vertexFormat.attributeDescriptions.size());
		for (const auto& desc : info.vertexFormat.attributeDescriptions) {
			VkVertexInputAttributeDescription vulkanAttribDesc{};
			vulkanAttribDesc.binding = 0;
			vulkanAttribDesc.location = desc.location;
			vulkanAttribDesc.offset = desc.offset;
			vulkanAttribDesc.format = vkinternal::getVertexAttribFormat(desc.format);
			attribDescs.push_back(vulkanAttribDesc);
		}

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";
		vertShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";
		fragShaderStageInfo.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo shaderStages[2] = { vertShaderStageInfo, fragShaderStageInfo };

		// set the vertex input layout
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attribDescs.size();
		vertexInputInfo.pVertexAttributeDescriptions = attribDescs.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = (float)pimpl->swapchain.extent.height;
		viewport.width = (float)pimpl->swapchain.extent.width;
		viewport.height = -(float)pimpl->swapchain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = pimpl->swapchain.extent;

		// Dynamic states removes the need to re-create pipelines whenever the window size changes
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		if (info.backfaceCulling == true) {
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		}
		else {
			rasterizer.cullMode = VK_CULL_MODE_NONE;
		}
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // ignored
		rasterizer.depthBiasClamp = 0.0f; // ignored
		rasterizer.depthBiasSlopeFactor = 0.0f; // ignored

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.minSampleShading = 1.0f; // ignored
		multisampling.pSampleMask = nullptr; // ignored
		multisampling.alphaToCoverageEnable = VK_FALSE; // ignored
		multisampling.alphaToOneEnable = VK_FALSE; // ignored

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		if (info.alphaBlending) {
			colorBlendAttachment.blendEnable = VK_TRUE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}
		else {
			colorBlendAttachment.blendEnable = VK_FALSE;
		}

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // ignored
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // ignored
		colorBlending.blendConstants[1] = 0.0f; // ignored
		colorBlending.blendConstants[2] = 0.0f; // ignored
		colorBlending.blendConstants[3] = 0.0f; // ignored

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};

		VkPushConstantRange pushConstantRange{};
		pushConstantRange.offset = 0;
		pushConstantRange.size = PUSH_CONSTANT_MAX_SIZE;
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts(info.descriptorSetLayouts.size());
		for (size_t i = 0; i < descriptorSetLayouts.size(); i++) {
			descriptorSetLayouts[i] = info.descriptorSetLayouts[i]->layout;
		}

		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = descriptorSetLayouts.size();
		layoutInfo.pSetLayouts = descriptorSetLayouts.data();
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstantRange;

		res = vkCreatePipelineLayout(pimpl->device.device, &layoutInfo, nullptr, &pipeline->layout);
		assert(res == VK_SUCCESS);

		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.stageCount = 2;
		createInfo.pStages = shaderStages;
		createInfo.pVertexInputState = &vertexInputInfo;
		createInfo.pInputAssemblyState = &inputAssembly;
		createInfo.pViewportState = &viewportState; // TODO: maybe this isn't needed?
		createInfo.pRasterizationState = &rasterizer;
		createInfo.pMultisampleState = &multisampling;
		createInfo.pDepthStencilState = &depthStencil;
		createInfo.pColorBlendState = &colorBlending;
		createInfo.pDynamicState = &dynamicState;
		createInfo.layout = pipeline->layout;
		createInfo.renderPass = pimpl->swapchain.renderpass;
		createInfo.subpass = 0;
		createInfo.basePipelineHandle = VK_NULL_HANDLE;
		createInfo.basePipelineIndex = -1;

		res = vkCreateGraphicsPipelines(pimpl->device.device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->handle);
		assert(res == VK_SUCCESS);

		vkDestroyShaderModule(pimpl->device.device, fragShaderModule, nullptr);
		vkDestroyShaderModule(pimpl->device.device, vertShaderModule, nullptr);

		return pipeline;

	}

	void GFXDevice::destroyPipeline(const gfx::Pipeline* pipeline)
	{
		vkDestroyPipeline(pimpl->device.device, pipeline->handle, nullptr);
		vkDestroyPipelineLayout(pimpl->device.device, pipeline->layout, nullptr);

		delete pipeline;
	}

	gfx::DescriptorSetLayout* GFXDevice::createDescriptorSetLayout()
	{
		gfx::DescriptorSetLayout* out = new gfx::DescriptorSetLayout{};

		std::vector<VkDescriptorSetLayoutBinding> bindings{};
		bindings.push_back({});
		bindings[0].binding = 0; // This should be as low as possible to avoid wasting memory
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1; // if > 1, accessible as an array in the shader
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // only accessible in vertex 

		VkDescriptorSetLayoutCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.bindingCount = bindings.size();
		info.pBindings = bindings.data();
		VKCHECK(vkCreateDescriptorSetLayout(pimpl->device.device, &info, nullptr, &out->layout));

		return out;
	}

	void GFXDevice::destroyDescriptorSetLayout(const gfx::DescriptorSetLayout* layout)
	{
		vkDestroyDescriptorSetLayout(pimpl->device.device, layout->layout, nullptr);
		delete layout;
	}

	gfx::DescriptorSet* GFXDevice::allocateDescriptorSet(const gfx::DescriptorSetLayout* layout)
	{
		gfx::DescriptorSet* set = new gfx::DescriptorSet{};

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			VkDescriptorSetAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = pimpl->descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &layout->layout
			};
			VkResult res;
			res = vkAllocateDescriptorSets(pimpl->device.device, &allocInfo, &set->sets[i]);
			if (res == VK_ERROR_FRAGMENTED_POOL) throw std::runtime_error("Descriptor pool is fragmented!");
			if (res == VK_ERROR_OUT_OF_POOL_MEMORY) throw std::runtime_error("Descriptor pool is out of memory!");
			VKCHECK(res);
		}

		return set;
	}

	void GFXDevice::updateDescriptor(const gfx::DescriptorSet* set, uint32_t binding, const gfx::DescriptorBuffer* buffer, size_t offset, size_t range)
	{
		assert(pimpl->FRAMECOUNT == 0);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			VkDescriptorBufferInfo bufferInfo{
			.buffer = buffer->gpuBuffers[i].buffer,
			.offset = offset,
			.range = range
			};
			VkWriteDescriptorSet descriptorWrite{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = set->sets[i],
				.dstBinding = binding,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &bufferInfo,
				.pTexelBufferView = nullptr
			};
			vkUpdateDescriptorSets(pimpl->device.device, 1, &descriptorWrite, 0, nullptr);
		}
	}

	gfx::DescriptorBuffer* GFXDevice::createDescriptorBuffer(uint64_t size, const void* initialData)
	{
		gfx::DescriptorBuffer* out = new gfx::DescriptorBuffer{};
		
		/* first make staging buffer */
		out->stagingBuffer.size = size;
		out->stagingBuffer.type = gfx::BufferType::UNIFORM;
		out->stagingBuffer.hostVisible = true;
		{
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = out->stagingBuffer.size;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			stagingBufferInfo.flags = 0;

			VmaAllocationCreateInfo stagingAllocInfo{};
			stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			VKCHECK(vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &out->stagingBuffer.buffer, &out->stagingBuffer.allocation, nullptr));

			void* dataDest;
			VKCHECK(vmaMapMemory(pimpl->allocator, out->stagingBuffer.allocation, &dataDest));
			memcpy(dataDest, initialData, out->stagingBuffer.size);
			vmaUnmapMemory(pimpl->allocator, out->stagingBuffer.allocation);
		}

		/* create the device-local set of buffers */
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {

			out->gpuBuffers[i].size = out->stagingBuffer.size;
			out->gpuBuffers[i].type = gfx::BufferType::UNIFORM;
			out->gpuBuffers[i].hostVisible = false;

			VkBufferCreateInfo gpuBufferInfo{};
			gpuBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			gpuBufferInfo.size = out->gpuBuffers[i].size;
			gpuBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			gpuBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			gpuBufferInfo.flags = 0;

			VmaAllocationCreateInfo gpuAllocationInfo{};
			gpuAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			gpuAllocationInfo.flags = 0;

			VKCHECK(vmaCreateBuffer(pimpl->allocator, &gpuBufferInfo, &gpuAllocationInfo, &out->gpuBuffers[i].buffer, &out->gpuBuffers[i].allocation, nullptr));

			/* copy staging buffer into both */
			copyBuffer(pimpl->device.device, pimpl->device.commandPools.transfer, pimpl->device.queues.transferQueues[0], out->stagingBuffer.buffer, out->gpuBuffers[i].buffer, out->stagingBuffer.size);
		}

		return out;

	}

	void GFXDevice::destroyDescriptorBuffer(const gfx::DescriptorBuffer* descriptorBuffer)
	{
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			vmaDestroyBuffer(pimpl->allocator, descriptorBuffer->gpuBuffers[i].buffer, descriptorBuffer->gpuBuffers[i].allocation);
		}

		vmaDestroyBuffer(pimpl->allocator, descriptorBuffer->stagingBuffer.buffer, descriptorBuffer->stagingBuffer.allocation);

		delete descriptorBuffer;
	}

	void GFXDevice::writeDescriptorBuffer(gfx::DescriptorBuffer* buffer, uint64_t offset, uint64_t size, const void* data)
	{
		assert(offset + size <= buffer->stagingBuffer.size);

		/* first update the staging buffer */
		void* dataDest;
		VKCHECK(vmaMapMemory(pimpl->allocator, buffer->stagingBuffer.allocation, &dataDest));
		memcpy(dataDest, (uint8_t*)data + offset, size);
		vmaUnmapMemory(pimpl->allocator, buffer->stagingBuffer.allocation);

		/* queue the writes to each gpu buffer */
		// This is required as buffers cannot be updated if they are currently in use
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			pimpl->descriptorBufferWriteQueues[i].insert(buffer);
		}

	}

	gfx::Buffer* GFXDevice::createBuffer(gfx::BufferType type, uint64_t size, const void* data)
	{
		[[maybe_unused]] VkResult res;

		auto out = new gfx::Buffer{};
		out->size = size;
		out->type = type;
		out->hostVisible = false;

		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;

		// first create the staging buffer
		{
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = out->size;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			stagingBufferInfo.flags = 0;

			VmaAllocationCreateInfo stagingAllocInfo{};
			stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			res = vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr);
			assert(res == VK_SUCCESS);

			void* dataDest;
			res = vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest);
			assert(res == VK_SUCCESS);
			memcpy(dataDest, data, out->size);
			vmaUnmapMemory(pimpl->allocator, stagingAllocation);
		}

		// create the actual buffer on the GPU
		{
			VkBufferCreateInfo gpuBufferInfo{};
			gpuBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			gpuBufferInfo.size = out->size;
			gpuBufferInfo.usage = vkinternal::getBufferUsageFlag(type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			gpuBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			gpuBufferInfo.flags = 0;

			VmaAllocationCreateInfo gpuAllocationInfo{};
			gpuAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			gpuAllocationInfo.flags = 0;

			res = vmaCreateBuffer(pimpl->allocator, &gpuBufferInfo, &gpuAllocationInfo, &out->buffer, &out->allocation, nullptr);
			assert(res == VK_SUCCESS);
		}

		// copy the data from the staging buffer to the gpu buffer
		copyBuffer(pimpl->device.device, pimpl->device.commandPools.transfer, pimpl->device.queues.transferQueues[0], stagingBuffer, out->buffer, out->size);

		// destroy staging buffer
		vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);
		return out;

	}

	void GFXDevice::destroyBuffer(const gfx::Buffer* buffer)
	{
		vmaDestroyBuffer(pimpl->allocator, buffer->buffer, buffer->allocation);
		delete buffer;
	}

	gfx::Texture* GFXDevice::createTexture(
		const void* imageData,
		uint32_t width,
		uint32_t height,
		gfx::TextureFilter minFilter,
		gfx::TextureFilter magFilter,
		gfx::MipmapSetting mipmapSetting,
		bool useAnisotropy)
	{
		(void)imageData;
		(void)width;
		(void)height;
		(void)minFilter;
		(void)magFilter;
		(void)mipmapSetting;
		(void)useAnisotropy;
		auto out = new gfx::Texture;

#if 0

		[[maybe_unused]] VkResult res;

		size_t imageSize = width * height * 4;

		if (mipmapSetting == gfx::MipmapSetting::OFF) {
			out->mipLevels = 1;
		}
		else {
			out->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		}

		// first load image into staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		{
			VkBufferCreateInfo stagingBufferInfo{};
			stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			stagingBufferInfo.size = imageSize;
			stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			stagingBufferInfo.flags = 0;

			VmaAllocationCreateInfo stagingAllocInfo{};
			stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			res = vmaCreateBuffer(pimpl->allocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr);
			assert(res == VK_SUCCESS);

			void* dataDest;
			res = vmaMapMemory(pimpl->allocator, stagingAllocation, &dataDest);
			assert(res == VK_SUCCESS);
			memcpy(dataDest, imageData, imageSize);
			vmaUnmapMemory(pimpl->allocator, stagingAllocation);
		}

		// create the image
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = out->mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo imageAllocInfo{};
		imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		res = vmaCreateImage(pimpl->allocator, &imageInfo, &imageAllocInfo, &out->image, &out->alloc, nullptr);
		assert(res == VK_SUCCESS);

		// transition the image layout
		{
			VkCommandBuffer commandBuffer = beginOneTimeCommands(pimpl->device, pimpl->commandPool);

			// begin cmd buffer

			cmdTransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, out->mipLevels, out->image);

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;

			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			// Mipmap generation handles the transition to SHADER_READ_ONLY_OPTIMAL
			cmdGenerateMipmaps(commandBuffer, out->image, width, height, out->mipLevels);

			// end cmd buffer
			endOneTimeCommands(pimpl->device, pimpl->commandPool, commandBuffer, pimpl->gfxQueue.handle);

		}

		// destroy staging buffer
		vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);

		// create image view
		VkImageViewCreateInfo imageViewInfo{};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = out->image;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageViewInfo.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = out->mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		res = vkCreateImageView(pimpl->device, &imageViewInfo, nullptr, &out->imageView);
		assert(res == VK_SUCCESS);

		VkFilter magFilterInternal = vkinternal::getTextureFilter(magFilter);
		VkFilter minFilterInternal = vkinternal::getTextureFilter(minFilter);

		// create texture sampler
		{

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = magFilterInternal;
			samplerInfo.minFilter = minFilterInternal;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			if (useAnisotropy) {
				samplerInfo.anisotropyEnable = VK_TRUE;
			}
			else {
				samplerInfo.anisotropyEnable = VK_FALSE;
			}
			samplerInfo.maxAnisotropy = pimpl->maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			if (mipmapSetting == gfx::MipmapSetting::LINEAR) {
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			}
			else {
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			}
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = static_cast<float>(out->mipLevels);
			samplerInfo.mipLodBias = 0.0f;

			res = vkCreateSampler(pimpl->device, &samplerInfo, nullptr, &out->sampler);
			assert(res == VK_SUCCESS);
		}

		// create descriptor pools
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = FRAMES_IN_FLIGHT;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = FRAMES_IN_FLIGHT;
		res = vkCreateDescriptorPool(pimpl->device, &poolInfo, nullptr, &out->pool);
		assert(res == VK_SUCCESS);

		std::array<VkDescriptorSetLayout, FRAMES_IN_FLIGHT> layouts{};
		layouts.fill(pimpl->samplerSetLayout);
		VkDescriptorSetAllocateInfo dSetAllocInfo{};
		dSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dSetAllocInfo.descriptorPool = out->pool;
		dSetAllocInfo.descriptorSetCount = FRAMES_IN_FLIGHT;
		dSetAllocInfo.pSetLayouts = layouts.data();
		res = vkAllocateDescriptorSets(pimpl->device, &dSetAllocInfo, out->descriptorSets.data());
		assert(res == VK_SUCCESS);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = out->imageView;
			imageInfo.sampler = out->sampler;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = out->descriptorSets[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(pimpl->device, 1, &descriptorWrite, 0, nullptr);
		}

#endif
		return out;
	}

	void GFXDevice::destroyTexture(const gfx::Texture* texture)
	{
		(void)texture;
#if 0
		vkDestroyDescriptorPool(pimpl->device, texture->pool, nullptr);
		vkDestroySampler(pimpl->device, texture->sampler, nullptr);
		vkDestroyImageView(pimpl->device, texture->imageView, nullptr);
		vmaDestroyImage(pimpl->allocator, texture->image, texture->alloc);
#endif
	}

	uint64_t GFXDevice::getFrameCount()
	{
		return pimpl->FRAMECOUNT;
	}

	void GFXDevice::waitIdle()
	{
		vkDeviceWaitIdle(pimpl->device.device);
	}

}
