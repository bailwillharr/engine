#include "scene.hpp"

#include "components/transform.hpp"
#include "components/renderable.hpp"
#include "systems/transform.hpp"
#include "systems/render.hpp"

namespace engine {

	Scene::Scene(Application* app)
		: m_app(app)
	{
		registerComponent<TransformComponent>();
		registerComponent<RenderableComponent>();
		registerSystem<TransformSystem>();
		registerSystem<RenderSystem>();
	}

	Scene::~Scene() {}

	uint32_t Scene::createEntity(const std::string& tag, uint32_t parent)
	{
		uint32_t id = m_nextEntityID++;

		m_signatures.emplace(id, std::bitset<MAX_COMPONENTS>{});

		auto t = addComponent<TransformComponent>(id);

		t->position = {0.0f, 0.0f, 0.0f};
		t->rotation = {};
		t->scale = {1.0f, 1.0f, 1.0f};

		t->tag = tag;
		t->parent = parent;

		return id;
	}

	uint32_t Scene::getEntity(const std::string& tag, uint32_t parent)
	{
		return getSystem<TransformSystem>()->getChildEntity(parent, tag);
	}

	size_t Scene::getComponentSignaturePosition(size_t hash)
	{
		return m_componentSignaturePositions.at(hash);
	}

	void Scene::update(float ts)
	{

		for (auto& [name, system] : m_systems) {
			system->onUpdate(ts);
		}

	}

}
