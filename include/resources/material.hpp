#pragma once

#include <memory>

namespace engine::resources {

	class Shader;
	class Texture;

	class Material {

	public:
		Material(std::shared_ptr<Shader> shader);
		~Material();
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;

		auto getShader() { return m_shader.get(); }

		std::shared_ptr<Texture> m_texture;

	private:
		const std::shared_ptr<Shader> m_shader;

	};

}
