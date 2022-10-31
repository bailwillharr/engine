#pragma once

#include "engine_api.h"

#include "gfx.hpp"

#include <memory>

struct SDL_Window;

namespace engine {
	
	class ENGINE_API GFXDevice {

	public:
		GFXDevice(const char* appName, const char* appVersion, SDL_Window* window);

		GFXDevice(const GFXDevice&) = delete;
		GFXDevice& operator=(const GFXDevice&) = delete;
		~GFXDevice();

		void getViewportSize(uint32_t *w, uint32_t *h);

		// adds a draw call to the queue
		// vertexBuffer is required, indexBuffer can be NULL, uniformData is required
		void draw(const gfx::Pipeline* pipeline, const gfx::Buffer* vertexBuffer, const gfx::Buffer* indexBuffer, uint32_t count, const void* pushConstantData);

		// Call once per frame. Executes all queued draw calls and renders to the screen.
		void renderFrame();
		
		// creates the equivalent of an OpenGL shader program & vertex attrib configuration
		gfx::Pipeline* createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat, uint64_t uniformBufferSize);
		void destroyPipeline(const gfx::Pipeline* pipeline);

		void updateUniformBuffer(const gfx::Pipeline* pipeline, void* data);

		gfx::Buffer* createBuffer(gfx::BufferType type, uint64_t size, const void* data);
		void destroyBuffer(const gfx::Buffer* buffer);

		// wait until all the active GPU queues have finished working
		void waitIdle();

	private:
		struct Impl;
		std::unique_ptr<Impl> pimpl;

	};

	extern GFXDevice* gfxdev;

}
