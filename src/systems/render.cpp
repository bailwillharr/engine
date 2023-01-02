#include "systems/render.hpp"

#include "application.hpp"
#include "gfx_device.hpp"

#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"

#include "resources/mesh.hpp"

#include <glm/mat4x4.hpp>

namespace engine {

	void RenderSystem::onUpdate(float ts)
	{
		GFXDevice* const gfx = m_scene->app()->gfx();

		constexpr float FOV_H = 75.0f;
		constexpr float CLIP_NEAR = 0.1f;
		constexpr float CLIP_FAR = 100.0f;

		uint32_t w, h;
		gfx->getViewportSize(&w, &h);
		float aspect = w / h;
		float verticalFOV = FOV_H / aspect;
		glm::mat4 projMatrix = glm::perspectiveZO(verticalFOV, aspect, CLIP_NEAR, CLIP_FAR);

		for (uint32_t entity : m_entities) {

			auto t = m_scene->getComponent<TransformComponent>(entity);
			auto r = m_scene->getComponent<RenderableComponent>(entity);

//			TRACE("rendering {}", t->tag);

			gfx->updateUniformBuffer(r->material->getShader()->getPipeline(), &projMatrix, sizeof(projMatrix), 0);

			struct {
				glm::mat4 model;
				glm::mat4 view;
			} pushConsts{};

			pushConsts.model = t->worldMatrix;
			pushConsts.view = glm::mat4{ 1.0f };

			gfx->draw(
				r->material->getShader()->getPipeline(),
				r->mesh->getVB(),
				r->mesh->getIB(),
				r->mesh->getCount(),
				&pushConsts,
				sizeof(pushConsts),
				r->material->m_texture->getHandle()
			);
		}

	}

}

