#include "components/camera.hpp"

#include "resource_manager.hpp"
#include "resources/shader.hpp"

#include "sceneroot.hpp"

#include "window.hpp"

#include "gfx_device.hpp"

#include "log.hpp"

namespace engine::components {

glm::vec4 Camera::s_clearColor{-999.0f, -999.0f, -999.0f, -999.0f};

Camera::Camera(Object* parent) : Component(parent, TypeEnum::CAMERA)
{
	parent->root.activateCam(getID());
}

Camera::~Camera()
{
	parent.root.deactivateCam(getID());
}

void Camera::updateCam(glm::mat4 transform, glm::mat4* viewMatOut)
{

	if (parent.win.getWindowResized()) {
		if (m_mode == Modes::PERSPECTIVE) {
			usePerspective(m_fovDeg);
		} else {
			useOrtho();
		}
	}

	glm::mat4 viewMatrix = glm::inverse(transform);

	resources::Shader::UniformBuffer uniformData{};

	uniformData.p = m_projMatrix;

	using namespace resources;

	auto resPtrs = parent.res.getAllResourcesOfType("shader");
	for (const auto& resPtr : resPtrs) {
		// shader ref count increased by 1, but only in this scope
		auto lockedPtr = resPtr.lock();
		auto shader = dynamic_cast<Shader*>(lockedPtr.get());
		// SET VIEW TRANSFORM HERE
		gfxdev->updateUniformBuffer(shader->getPipeline(), &uniformData.p, sizeof(uniformData.p), offsetof(resources::Shader::UniformBuffer, p));
	}

	*viewMatOut = viewMatrix;

}

static glm::vec2 getViewportSize()
{
	uint32_t width;
	uint32_t height;
	gfxdev->getViewportSize(&width, &height);
	return {width, height};
}

void Camera::usePerspective(float fovDeg)
{
	constexpr float NEAR = 0.1f;
	constexpr float FAR = 100000.0f;

	m_mode = Modes::PERSPECTIVE;
	m_fovDeg = fovDeg;

	glm::vec2 viewportDim = getViewportSize();

	float fovRad = glm::radians(fovDeg);
	m_projMatrix = glm::perspectiveFovRH_ZO(fovRad, viewportDim.x, viewportDim.y, NEAR, FAR);
}

void Camera::useOrtho()
{
	m_mode = Modes::ORTHOGRAPHIC;

	glm::vec2 viewportDim = getViewportSize();
	float aspect = viewportDim.x / viewportDim.y;

	m_projMatrix = glm::orthoRH_ZO(-10.0f * aspect, 10.0f * aspect, -10.0f, 10.0f, -100.0f, 100.0f);
}

}
