// The implementation of the graphics layer using Vulkan 1.3.
// This uses SDL specific code

#ifdef ENGINE_BUILD_NULLGFX

#include "gfx_device.hpp"
#include "util.hpp"
#include "config.h"
#include "log.hpp"

namespace engine {

	// class definitions

	struct GFXDevice::Impl {
		
	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		pimpl = std::make_unique<Impl>();
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
		return true;
	}

	void GFXDevice::waitIdle()
	{
	}

}

#endif
