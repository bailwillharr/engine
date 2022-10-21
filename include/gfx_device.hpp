#pragma once

#include "engine_api.h"

#include <memory>

struct SDL_Window;

namespace engine {

	namespace gfx {

		enum class BufferUsage {
			DEFAULT,
			UPLOAD,
			READBACK,
		};

		enum class BindFlag {
			NONE = 0,
			UNIFORM_BUFFER = 1 << 0,
		};

		struct BufferDesc {
			uint64_t size;
			BufferUsage usage;
			BindFlag bindFlags;
		};

		// handles (incomplete types)

		class BufferHandle;

	};
	
	class ENGINE_API GFXDevice {

	public:
		GFXDevice(const char* appName, const char* appVersion, SDL_Window* window);

		GFXDevice(const GFXDevice&) = delete;
		GFXDevice& operator=(const GFXDevice&) = delete;
		~GFXDevice();

		// Call once per frame. Executes all queued draw calls and renders to the screen.
		void draw();
		
		// creates the equivalent of an OpenGL shader program & vertex attrib configuration
		void createPipeline(const char* vertShaderPath, const char* fragShaderPath);

		// creates a vertex array for holding mesh data
		bool createBuffer(const gfx::BufferDesc& desc, const void* data, gfx::BufferHandle* out);

		// wait until all the active GPU queues have finished working
		void waitIdle();

	private:
		struct Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
