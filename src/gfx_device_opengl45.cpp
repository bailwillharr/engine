// The implementation of the graphics layer using Vulkan 1.3.
// This uses SDL specific code

#ifdef ENGINE_BUILD_OPENGL

#include "gfx_device.hpp"
#include "util.hpp"
#include "config.h"
#include "log.hpp"

#include <glad/glad.h>

#include <SDL2/SDL.h>

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>

namespace engine {

	// structures and enums

	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (file.is_open() == false) {
			throw std::runtime_error("Unable to open file " + filename);
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0);
		file.read(buffer.data(), buffer.size());
		file.close();
		return buffer;
	}



	// class definitions

	struct GFXDevice::Impl {

		SDL_GLContext context = nullptr;
		
	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		pimpl = std::make_unique<Impl>();

		pimpl->context = SDL_GL_CreateContext(window);

	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");
	}

	void GFXDevice::draw()
	{
	}
	
	void GFXDevice::createPipeline(const char* vertShaderPath, const char* fragShaderPath)
	{
	}

	bool GFXDevice::createBuffer(const gfx::BufferDesc& desc, const void* data, gfx::BufferHandle* out)
	{
		return false;
	}

	void GFXDevice::waitIdle()
	{
		glFinish();
	}

}

#endif
