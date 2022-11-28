// This implementation of the graphics layer does nothing

//#define ENGINE_BUILD_NULLGFX
#ifdef ENGINE_BUILD_NULLGFX

#include "gfx_device.hpp"
#include "util.hpp"
#include "config.h"
#include "log.hpp"

#include <SDL2/SDL.h>

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>

namespace engine {

	// structures and enums

	// class definitions

	struct GFXDevice::Impl {

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window, bool vsync)
	{
		pimpl = std::make_unique<Impl>();
	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");
	}

	void GFXDevice::drawBuffer(const gfx::Pipeline* pipeline, const gfx::Buffer* vertexBuffer, uint32_t count)
	{
	}

	void GFXDevice::drawIndexed(const gfx::Pipeline* pipeline, const gfx::Buffer* vertexBuffer, const gfx::Buffer* indexBuffer, uint32_t indexCount)
	{
	}

	void GFXDevice::renderFrame()
	{

	}
		
	gfx::Pipeline* GFXDevice::createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat)
	{
		return nullptr;
	}

	void GFXDevice::destroyPipeline(const gfx::Pipeline* pipeline)
	{

	}

	gfx::Buffer* GFXDevice::createBuffer(gfx::BufferType type, uint64_t size, const void* data)
	{
		return nullptr;
	}

	void GFXDevice::destroyBuffer(const gfx::Buffer* buffer)
	{

	}

	void GFXDevice::waitIdle()
	{
	}

}

#endif
