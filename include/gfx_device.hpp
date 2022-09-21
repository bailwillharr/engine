#pragma once

#include "engine_api.h"

#include "engine.hpp"

#include <memory>

struct SDL_Window;

namespace engine::gfx {
	
	struct ENGINE_API GFXDevice {

		GFXDevice(AppInfo appInfo, SDL_Window* window);

		GFXDevice(const GFXDevice&) = delete;
		GFXDevice& operator=(const GFXDevice&) = delete;
		~GFXDevice();

	private:
		class Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
