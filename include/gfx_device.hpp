#pragma once

#include "engine_api.h"

#include <memory>

struct SDL_Window;

namespace engine {
	
	class ENGINE_API GFXDevice {

	public:
		GFXDevice(const char* appName, const char* appVersion, SDL_Window* window);

		GFXDevice(const GFXDevice&) = delete;
		GFXDevice& operator=(const GFXDevice&) = delete;
		~GFXDevice();

		// submit command lists and draw to the screen
		void draw();

	private:
		class Impl;
		std::unique_ptr<Impl> m_pimpl;

	};

}
