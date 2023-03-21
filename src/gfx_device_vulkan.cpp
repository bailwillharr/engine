// The implementation of the graphics layer using Vulkan 1.3.

/* IMPORTANT INFORMATION
 * 
 * When allocating memory with  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, always set a memory priority.
 * This feature uses the device extension VK_EXT_memory_priority. Depth buffers have a priority of 0.9f.
 * Other, non-essential allocations will have a priority of 0.5f.
 * 
 */

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
		VkSemaphore transferSemaphore = VK_NULL_HANDLE;
		VkSemaphore presentSemaphore = VK_NULL_HANDLE;
		VkSemaphore renderSemaphore = VK_NULL_HANDLE;

		VkCommandPool graphicsPool = VK_NULL_HANDLE;
		VkCommandBuffer drawBuf = VK_NULL_HANDLE;

		VkCommandPool transferPool = VK_NULL_HANDLE;
		VkCommandBuffer transferBuf = VK_NULL_HANDLE;
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
		FrameData frameData{};
		uint32_t currentFrameIndex = 0; // corresponds to the frameData
		uint32_t imageIndex = 0; // for swapchain present
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
		std::array<VkSemaphore, FRAMES_IN_FLIGHT> copySemaphores;
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

		VkCommandPool transferCommandPool = VK_NULL_HANDLE;

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

		pimpl->instance = createVulkanInstance(pimpl->window, appName, appVersion, pimpl->graphicsSettings.enableValidation, MessageSeverity::SEV_WARNING);

		if (SDL_Vulkan_CreateSurface(pimpl->window, pimpl->instance.instance, &pimpl->surface) == false) {
			throw std::runtime_error("Unable to create window surface");
		};

		DeviceRequirements deviceRequirements{};
		deviceRequirements.requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		deviceRequirements.optionalExtensions = { VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME };
		deviceRequirements.requiredFeatures.samplerAnisotropy = VK_TRUE;
		deviceRequirements.requiredFeatures.fillModeNonSolid = VK_TRUE;
		deviceRequirements.formats.push_back(
			FormatRequirements{
				.format = VK_FORMAT_R8G8B8A8_SRGB,
				.properties = VkFormatProperties{
					.linearTilingFeatures = {},
					.optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
					.bufferFeatures = {},
				}
			}
		);
		deviceRequirements.formats.push_back(
			FormatRequirements{
				.format = VK_FORMAT_R32G32_SFLOAT,
				.properties = VkFormatProperties{
					.linearTilingFeatures = {},
					.optimalTilingFeatures = {},
					.bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
				}
			}
		);
		deviceRequirements.formats.push_back(
			FormatRequirements{
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.properties = VkFormatProperties{
					.linearTilingFeatures = {},
					.optimalTilingFeatures = {},
					.bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
				}
			}
		);
		deviceRequirements.formats.push_back(
			FormatRequirements{
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.properties = VkFormatProperties{
					.linearTilingFeatures = {},
					.optimalTilingFeatures = {},
					.bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
				}
			}
		);
		deviceRequirements.formats.push_back( // Depth buffer format
			FormatRequirements{
				.format = VK_FORMAT_D16_UNORM,
				.properties = VkFormatProperties{
					.linearTilingFeatures = {},
					.optimalTilingFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
					.bufferFeatures = {},
				}
			}
		);

		pimpl->device = createDevice(pimpl->instance.instance, deviceRequirements, pimpl->surface);

		pimpl->allocator = createAllocator(pimpl->instance.instance, pimpl->device.device, pimpl->device.physicalDevice);

		pimpl->swapchainInfo.device = pimpl->device.device;
		pimpl->swapchainInfo.allocator = pimpl->allocator;
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
			VKCHECK(vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].transferSemaphore));
			VKCHECK(vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].presentSemaphore));
			VKCHECK(vkCreateSemaphore(pimpl->device.device, &smphInfo, nullptr, &pimpl->frameData[i].renderSemaphore));

			VkCommandPoolCreateInfo poolInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0, // Command buffers cannot be individually reset (more performant this way)
				.queueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily
			};
			VKCHECK(vkCreateCommandPool(pimpl->device.device, &poolInfo, nullptr, &pimpl->frameData[i].graphicsPool));
			poolInfo.queueFamilyIndex = pimpl->device.queues.transferQueueFamily;
			VKCHECK(vkCreateCommandPool(pimpl->device.device, &poolInfo, nullptr, &pimpl->frameData[i].transferPool));

			VkCommandBufferAllocateInfo cmdAllocInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = pimpl->frameData[i].graphicsPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1
			};
			VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &cmdAllocInfo, &pimpl->frameData[i].drawBuf));
			cmdAllocInfo.commandPool = pimpl->frameData[i].transferPool;
			VKCHECK(vkAllocateCommandBuffers(pimpl->device.device, &cmdAllocInfo, &pimpl->frameData[i].transferBuf));
		}

		/* create command pool for one-off transfer operations */
		VkCommandPoolCreateInfo transferPoolInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, // These command buffers don't last very long
			.queueFamilyIndex = pimpl->device.queues.transferQueueFamily
		};
		VKCHECK(vkCreateCommandPool(pimpl->device.device, &transferPoolInfo, nullptr, &pimpl->transferCommandPool));

		/* create a global descriptor pool */
		std::vector<VkDescriptorPoolSize> poolSizes{};
		poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100u }); // purposely low limit

		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.pNext = nullptr;
		descriptorPoolInfo.flags = 0;
		descriptorPoolInfo.maxSets = 100; // purposely low limit
		descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		VKCHECK(vkCreateDescriptorPool(pimpl->device.device, &descriptorPoolInfo, nullptr, &pimpl->descriptorPool));

	}

	GFXDevice::~GFXDevice()
	{
		vkDestroyDescriptorPool(pimpl->device.device, pimpl->descriptorPool, nullptr);

		vkDestroyCommandPool(pimpl->device.device, pimpl->transferCommandPool, nullptr);

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			vkDestroyCommandPool(pimpl->device.device, pimpl->frameData[i].graphicsPool, nullptr);
			vkDestroyCommandPool(pimpl->device.device, pimpl->frameData[i].transferPool, nullptr);
			vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].renderSemaphore, nullptr);
			vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].presentSemaphore, nullptr);
			vkDestroySemaphore(pimpl->device.device, pimpl->frameData[i].transferSemaphore, nullptr);
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

	/* TODO: the descriptor buffer copy command buffers are never freed.
	 * This causes the program to crash on IGPU after around 1 minute. (VK_ERROR_DEVICE_LOST on a vkQueueSubmit) */
	gfx::DrawBuffer* GFXDevice::beginRender()
	{
		VkResult res;

		const uint32_t currentFrameIndex = pimpl->FRAMECOUNT % FRAMES_IN_FLIGHT;
		const FrameData frameData = pimpl->frameData[currentFrameIndex];

		/* wait until the previous frame RENDERING has finished */
		res = vkWaitForFences(pimpl->device.device, 1, &frameData.renderFence, VK_TRUE, 1000000000LL);
		VKCHECK(res);
		res = vkResetFences(pimpl->device.device, 1, &frameData.renderFence);
		VKCHECK(res);

		/* perform any pending uniform buffer writes */
		VKCHECK(vkResetCommandPool(pimpl->device.device, frameData.transferPool, 0));

		VkCommandBufferBeginInfo transferBeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr // ignored
		};
		VKCHECK(vkBeginCommandBuffer(frameData.transferBuf, &transferBeginInfo));

		// transfer cmds...

		std::vector<VkBufferMemoryBarrier2> barriers{};
		for (gfx::DescriptorBuffer* descriptorBuffer : pimpl->descriptorBufferWriteQueues[currentFrameIndex]) {

			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = descriptorBuffer->stagingBuffer.size;
			vkCmdCopyBuffer(
				frameData.transferBuf,
				descriptorBuffer->stagingBuffer.buffer,
				descriptorBuffer->gpuBuffers[currentFrameIndex].buffer,
				1,
				&copyRegion
			);

			VkBufferMemoryBarrier2& barrier = barriers.emplace_back();
			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.dstAccessMask = 0;
			barrier.srcQueueFamilyIndex = pimpl->device.queues.transferQueueFamily;
			barrier.dstQueueFamilyIndex = pimpl->device.queues.presentAndDrawQueueFamily;
			barrier.buffer = descriptorBuffer->gpuBuffers[currentFrameIndex].buffer;
			barrier.offset = 0;
			barrier.size = descriptorBuffer->gpuBuffers[currentFrameIndex].size;
		}
		pimpl->descriptorBufferWriteQueues[currentFrameIndex].clear();

		VkDependencyInfo dependencyInfo{};
		dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependencyInfo.bufferMemoryBarrierCount = (uint32_t)barriers.size();
		dependencyInfo.pBufferMemoryBarriers = barriers.data();
		vkCmdPipelineBarrier2(frameData.transferBuf, &dependencyInfo);

		VKCHECK(vkEndCommandBuffer(frameData.transferBuf));

		VkSubmitInfo transferSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 0, // needs to wait for render but the fence does that
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &frameData.transferBuf,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &frameData.transferSemaphore,
		}; 
		res = vkQueueSubmit(pimpl->device.queues.transferQueues[0], 1, &transferSubmitInfo, VK_NULL_HANDLE);
		assert(res == VK_SUCCESS);

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

		/* record command buffer */
		res = vkResetCommandPool(pimpl->device.device, frameData.graphicsPool, 0);
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

			/* change barriers to perform a queue ownership acquire operation */
			for (VkBufferMemoryBarrier2& barrier : barriers) {
				barrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
				barrier.srcAccessMask = 0;
				barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
				barrier.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT;
			}
			vkCmdPipelineBarrier2(frameData.drawBuf, &dependencyInfo);

			std::array<VkClearValue, 2> clearValues{}; // Using same value for all components enables compression according to NVIDIA Best Practices
			clearValues[0].color.float32[0] = 1.0f;
			clearValues[0].color.float32[1] = 1.0f;
			clearValues[0].color.float32[2] = 1.0f;
			clearValues[0].color.float32[3] = 1.0f;
			clearValues[1].depthStencil.depth = 1.0f;

			VkRenderPassBeginInfo passBegin{};
			passBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passBegin.pNext = nullptr;
			passBegin.renderPass = pimpl->swapchain.renderpass;
			passBegin.framebuffer = pimpl->swapchain.framebuffers[swapchainImageIndex];
			passBegin.renderArea.extent = pimpl->swapchain.extent;
			passBegin.renderArea.offset = { 0, 0 };
			passBegin.clearValueCount = (uint32_t)clearValues.size();
			passBegin.pClearValues = clearValues.data();
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
		assert(drawBuffer != nullptr);

		uint32_t swapchainImageIndex = drawBuffer->imageIndex;
		VkResult res;

		vkCmdEndRenderPass(drawBuffer->frameData.drawBuf);

		res = vkEndCommandBuffer(drawBuffer->frameData.drawBuf);
		VKCHECK(res);

		// SUBMIT

		std::vector<VkSemaphore> waitSemaphores{};
		std::vector<VkPipelineStageFlags> waitDstStageMasks{};

		waitSemaphores.push_back(drawBuffer->frameData.presentSemaphore);
		waitDstStageMasks.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		waitSemaphores.push_back(drawBuffer->frameData.transferSemaphore);
		waitDstStageMasks.push_back(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = (uint32_t)waitSemaphores.size(),
			.pWaitSemaphores = waitSemaphores.data(),
			.pWaitDstStageMask = waitDstStageMasks.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &drawBuffer->frameData.drawBuf,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &drawBuffer->frameData.renderSemaphore,
		}; 
		res = vkQueueSubmit(pimpl->device.queues.drawQueues[0], 1, &submitInfo, drawBuffer->frameData.renderFence);
		assert(res == VK_SUCCESS);
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
		else if (res != VK_SUCCESS) throw std::runtime_error("Failed to queue present! Code: " + std::to_string(res));

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
			vulkanAttribDesc.location = desc.location;
			vulkanAttribDesc.binding = 0;
			vulkanAttribDesc.format = vkinternal::getVertexAttribFormat(desc.format);
			vulkanAttribDesc.offset = desc.offset;
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
		rasterizer.rasterizerDiscardEnable = VK_FALSE; // enabling this will not run the fragment shaders at all
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
		layoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
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
		info.bindingCount = (uint32_t)bindings.size();
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

			VkSemaphoreCreateInfo semInfo{};
			semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			semInfo.pNext = nullptr;
			semInfo.flags = 0;
			VKCHECK(vkCreateSemaphore(pimpl->device.device, &semInfo, nullptr, &out->copySemaphores[i]));

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
			copyBuffer(pimpl->device.device, pimpl->transferCommandPool, pimpl->device.queues.transferQueues[0], out->stagingBuffer.buffer, out->gpuBuffers[i].buffer, out->stagingBuffer.size);
		}

		return out;

	}

	void GFXDevice::destroyDescriptorBuffer(const gfx::DescriptorBuffer* descriptorBuffer)
	{
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
			vmaDestroyBuffer(pimpl->allocator, descriptorBuffer->gpuBuffers[i].buffer, descriptorBuffer->gpuBuffers[i].allocation);
			vkDestroySemaphore(pimpl->device.device, descriptorBuffer->copySemaphores[i], nullptr);
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
		copyBuffer(pimpl->device.device, pimpl->transferCommandPool, pimpl->device.queues.transferQueues[0], stagingBuffer, out->buffer, out->size);

		// destroy staging buffer
		vmaDestroyBuffer(pimpl->allocator, stagingBuffer, stagingAllocation);
		return out;

	}

	void GFXDevice::destroyBuffer(const gfx::Buffer* buffer)
	{
		vmaDestroyBuffer(pimpl->allocator, buffer->buffer, buffer->allocation);
		delete buffer;
	}

	gfx::Image* GFXDevice::createImage(uint32_t w, uint32_t h, const void* imageData)
	{
		(void)w;
		(void)h;
		(void)imageData;
		return nullptr;
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

		return out;
	}

	void GFXDevice::destroyTexture(const gfx::Texture* texture)
	{
		(void)texture;
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