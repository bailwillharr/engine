#include "components/camera.hpp"

#include "resource_manager.hpp"
#include "resources/shader.hpp"

#include "sceneroot.hpp"

#include "window.hpp"

#include "log.hpp"

static const std::string VIEW_MAT_UNIFORM = "viewMat";
static const std::string PROJ_MAT_UNIFORM = "projMat";
static const std::string WINDOW_SIZE_UNIFORM = "windowSize";

namespace components {

glm::vec4 Camera::s_clearColor{-999.0f, -999.0f, -999.0f, -999.0f};

Camera::Camera(Object* parent) : Component(parent, TypeEnum::CAMERA)
{
	parent->root.activateCam(getID());
	glEnable(GL_DEPTH_TEST);

	glDisable(GL_STENCIL_TEST);

	glEnable(GL_CULL_FACE);
}

Camera::~Camera()
{
	parent.root.deactivateCam(getID());
}

void Camera::updateCam(glm::mat4 transform)
{

	if (parent.win.getWindowResized()) {
		if (m_mode == Modes::PERSPECTIVE) {
			usePerspective(m_fovDeg);
		} else {
			useOrtho();
		}
	}

	glm::mat4 viewMatrix = glm::inverse(transform);

	using namespace resources;

	auto resPtrs = parent.res.getAllResourcesOfType("shader");
	for (const auto& resPtr : resPtrs) {
		// shader ref count increased by 1, but only in this scope
		auto lockedPtr = resPtr.lock();
		auto shader = dynamic_cast<Shader*>(lockedPtr.get());
		shader->setUniform_m4(VIEW_MAT_UNIFORM, viewMatrix);
		shader->setUniform_m4(PROJ_MAT_UNIFORM, m_projMatrix);
		shader->setUniform_v2(WINDOW_SIZE_UNIFORM, win.getViewportSize());
		shader->setUniform_v3("lightPos", m_lightPos);
	}

	if (s_clearColor != clearColor) {
		s_clearColor = clearColor;
		glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
	}

	glClear((m_noClear ? 0 : GL_COLOR_BUFFER_BIT) | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

}

static glm::vec2 getViewportSize()
{
	GLint64 viewportParams[4];
	glGetInteger64v(GL_VIEWPORT, viewportParams);
	return { viewportParams[2], viewportParams[3] };
}

void Camera::usePerspective(float fovDeg)
{
	constexpr float NEAR = 0.1f;
	constexpr float FAR = 100000.0f;

	m_mode = Modes::PERSPECTIVE;
	m_fovDeg = fovDeg;

	glm::vec2 viewportDim = getViewportSize();

	float fovRad = glm::radians(fovDeg);
	m_projMatrix = glm::perspectiveFov(fovRad, viewportDim.x, viewportDim.y, NEAR, FAR);
}

void Camera::useOrtho()
{
	m_mode = Modes::ORTHOGRAPHIC;

	glm::vec2 viewportDim = getViewportSize();
	float aspect = viewportDim.x / viewportDim.y;

	m_projMatrix = glm::ortho(-10.0f * aspect, 10.0f * aspect, -10.0f, 10.0f, -100.0f, 100.0f);
}

}
