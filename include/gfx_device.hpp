#pragma once

#include "gfx.hpp"

#include <memory>

struct SDL_Window; // <SDL_video.h>

namespace engine {
	
	class GFXDevice {

	public:
		GFXDevice(const char* appName, const char* appVersion, SDL_Window* window, gfx::GraphicsSettings settings);

		GFXDevice(const GFXDevice&) = delete;
		GFXDevice& operator=(const GFXDevice&) = delete;
		~GFXDevice();

		void getViewportSize(uint32_t *w, uint32_t *h);

		gfx::DrawBuffer* beginRender();
		void finishRender(gfx::DrawBuffer* drawBuffer);

		void cmdBindPipeline(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline);
		void cmdBindVertexBuffer(gfx::DrawBuffer* drawBuffer, uint32_t binding, const gfx::Buffer* buffer);
		void cmdBindIndexBuffer(gfx::DrawBuffer* drawBuffer, const gfx::Buffer* buffer);
		void cmdDrawIndexed(gfx::DrawBuffer* drawBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
		void cmdPushConstants(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline, uint32_t offset, uint32_t size, const void* data);
		void cmdBindDescriptorSet(gfx::DrawBuffer* drawBuffer, const gfx::Pipeline* pipeline, const gfx::DescriptorSet* set, uint32_t setNumber);
		
		gfx::Pipeline* createPipeline(const gfx::PipelineInfo& info);
		void destroyPipeline(const gfx::Pipeline* pipeline);

		gfx::DescriptorSetLayout* createDescriptorSetLayout(const std::vector<gfx::DescriptorSetLayoutBinding>& bindings);
		void destroyDescriptorSetLayout(const gfx::DescriptorSetLayout* layout);
		
		gfx::DescriptorSet* allocateDescriptorSet(const gfx::DescriptorSetLayout* layout);
		// This updates all copies of the descriptor. This cannot be used after any frames have been renderered
		void updateDescriptorUniformBuffer(const gfx::DescriptorSet* set, uint32_t binding, const gfx::UniformBuffer* buffer, size_t offset, size_t range);
		void updateDescriptorCombinedImageSampler(const gfx::DescriptorSet* set, uint32_t binding, const gfx::Image* image, const gfx::Sampler* sampler);

		gfx::UniformBuffer* createUniformBuffer(uint64_t size, const void* initialData);
		void destroyUniformBuffer(const gfx::UniformBuffer* descriptorBuffer);

		void writeUniformBuffer(gfx::UniformBuffer* buffer, uint64_t offset, uint64_t size, const void* data);

		// Loads data into staging buffer and copies that into a single GPU buffer.
		gfx::Buffer* createBuffer(gfx::BufferType type, uint64_t size, const void* data);
		void destroyBuffer(const gfx::Buffer* buffer);

		gfx::Image* createImage(uint32_t w, uint32_t h, const void* imageData);
		void destroyImage(const gfx::Image* image);

		gfx::Sampler* createSampler();
		void destroySampler(const gfx::Sampler* sampler);

		uint64_t getFrameCount();

		void logPerformanceInfo();

		// wait until all the active GPU queues have finished working
		void waitIdle();

	private:
		struct Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
