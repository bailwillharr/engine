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

		gfx::CommandBuffer* beginRender();
		void finishRender(gfx::CommandBuffer* commandBuffer);

		void cmdBindPipeline(gfx::CommandBuffer* commandBuffer, const gfx::Pipeline* pipeline);
		void cmdBindDescriptorSetTexture(gfx::CommandBuffer* commandBuffer, uint32_t set, uint32_t binding, const gfx::Texture* texture);
		void cmdBindDescriptorSetBuffer(gfx::CommandBuffer* commandBuffer, uint32_t set, uint32_t binding, const gfx::Texture* texture);
		
		// creates the equivalent of an OpenGL shader program & vertex attrib configuration
		gfx::Pipeline* createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat, uint64_t uniformBufferSize, bool alphaBlending, bool backfaceCulling);
		void destroyPipeline(const gfx::Pipeline* pipeline);

		void updateUniformBuffer(const gfx::Pipeline* pipeline, const void* data, size_t size, uint32_t offset);

		gfx::Buffer* createBuffer(gfx::BufferType type, uint64_t size, const void* data);
		void destroyBuffer(const gfx::Buffer* buffer);

		gfx::Texture* createTexture(
			const void* imageData,
			uint32_t width,
			uint32_t height,
			gfx::TextureFilter minFilter,
			gfx::TextureFilter magFilter,
			gfx::MipmapSetting mipmapSetting,
			bool useAnisotropy = false);
		void destroyTexture(const gfx::Texture* texture);

		// wait until all the active GPU queues have finished working
		void waitIdle();

	private:
		struct Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
