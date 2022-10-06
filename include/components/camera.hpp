#pragma once

#include "engine_api.h"

#include "component.hpp"

#include "object.hpp"

#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace engine::components {

class ENGINE_API Camera : public Component {

public:
	Camera(Object* parent);
	~Camera();

	// called every frame, don't call manually
	void updateCam(glm::mat4 transform);

	void makeActive();
	bool isActive();

	void usePerspective(float fovDeg);
	void useOrtho();

	bool m_noClear = false;

	glm::vec3 m_lightPos = { 0.0f, 100.0f, 0.0f };

	glm::vec4 clearColor{0.1f, 0.1f, 0.1f, 1.0f};

	glm::mat4 m_projMatrix{ 1.0f };

private:

	enum class Modes {
		PERSPECTIVE,
		ORTHOGRAPHIC
	};

	Modes m_mode = Modes::ORTHOGRAPHIC;

	// perspective mode settings
	float m_fovDeg = 90.0f;

	static glm::vec4 s_clearColor;

};

}
