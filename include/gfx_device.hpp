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

		// submit command lists and draw to the screen
		void draw();

		bool createBuffer(const gfx::BufferDesc& desc, const void* data, gfx::BufferHandle* out);

	private:
		class Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
