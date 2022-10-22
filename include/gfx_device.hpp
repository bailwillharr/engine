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

		// adds a vertex buffer draw call to the queue
		void drawBuffer(const gfx::VertexBuffer* vb);

		// Call once per frame. Executes all queued draw calls and renders to the screen.
		void draw();
		
		// creates the equivalent of an OpenGL shader program & vertex attrib configuration
		void createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat);

		// creates a vertex array for holding mesh data
		gfx::VertexBuffer* createVertexBuffer(uint32_t size, const void* vertices, const void* indices);
		void destroyVertexBuffer(const gfx::VertexBuffer* buffer);

		// wait until all the active GPU queues have finished working
		void waitIdle();

	private:
		struct Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
