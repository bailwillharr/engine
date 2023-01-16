#pragma once

#include <memory>

namespace engine::resources {

	class Shader;
	class Texture;

	// copyable
	class Material {

	public:
		Material(std::shared_ptr<Shader> shader);
		~Material() = default;
		Material(const Material&);
		Material& operator=(const Material&) = delete;

		auto getShader() { return m_shader.get(); }

		std::shared_ptr<Texture> m_texture;

	private:
		const std::shared_ptr<Shader> m_shader;

	};

}
