#pragma once

#include "resource_manager.hpp"

namespace engine {

	class Texture {

	};

	class TextureManager : public ResourceManager<Texture> {

	public:
		TextureManager();
		~TextureManager() override;
		TextureManager(const TextureManager&) = delete;
		TextureManager& operator=(const TextureManager&) = delete;

	private:

	};

}