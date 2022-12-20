#pragma once

#include <memory>

namespace engine::resources {

	class Shader;

	class Material {

	public:
		Material(std::shared_ptr<Shader> shader);
		~Material();
		Material(const Material&) = delete;
		Material& operator=(const Material&) = delete;

		Shader* getShader() { return m_shader.get(); }

	private:
		const std::shared_ptr<Shader> m_shader;

	};

}
