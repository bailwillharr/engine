#pragma once

#include "engine_api.h"

#include "engine.hpp"

#include <memory>

struct SDL_Window;

namespace engine::gfx {
	
	struct ENGINE_API Device {

		Device(AppInfo appInfo, SDL_Window* window);
		~Device();

	private:
		class Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
