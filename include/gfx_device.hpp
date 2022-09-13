#pragma once

#include "engine_api.h"

#include "engine.hpp"

class Window;

namespace engine::gfx {
	
	struct ENGINE_API Device {

		Device(AppInfo appInfo, const Window& window);
		~Device();

	private:
		class Impl;
		std::unique_ptr<Impl> pimpl;

	};

}
