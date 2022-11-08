#pragma once

#if 0

#include "engine_api.h"

#include "resource.hpp"


namespace engine::resources {

class ENGINE_API Texture : public Resource {

private:

public:
	Texture(const std::filesystem::path& resPath);
	~Texture() override;

	void bindTexture() const;

	static void invalidate();
};

}

#endif