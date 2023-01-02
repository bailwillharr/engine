#pragma once

#include <memory>

namespace engine {

	namespace resources {
		class Mesh;
		class Material;
	}

	struct RenderableComponent {
		std::shared_ptr<resources::Mesh> mesh;
		std::shared_ptr<resources::Material> material;
	};

}
