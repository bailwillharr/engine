#include "scene.h"

#include "components/transform.h"
#include "components/renderable.h"
#include "components/collider.h"
#include "components/custom.h"
#include "systems/transform.h"
#include "systems/render.h"
#include "systems/collisions.h"
#include "systems/custom_behaviour.h"

namespace engine {

Scene::Scene(Application* app) : app_(app) {
  // event system
  event_system_ = std::make_unique<EventSystem>();

  // ecs configuration:

  RegisterComponent<TransformComponent>();
  RegisterComponent<RenderableComponent>();
  RegisterComponent<ColliderComponent>();
  RegisterComponent<CustomComponent>();

  // Order here matters:
  RegisterSystem<TransformSystem>();
  RegisterSystem<PhysicsSystem>();
  RegisterSystem<RenderSystem>();
  RegisterSystem<CustomBehaviourSystem>();
}

Scene::~Scene() {}

uint32_t Scene::CreateEntity(const std::string& tag, uint32_t parent) {
  uint32_t id = next_entity_id_++;

  signatures_.emplace(id, std::bitset<kMaxComponents>{});

  auto t = AddComponent<TransformComponent>(id);

  t->position = {0.0f, 0.0f, 0.0f};
  t->rotation = {};
  t->scale = {1.0f, 1.0f, 1.0f};

  t->tag = tag;
  t->parent = parent;

  return id;
}

uint32_t Scene::getEntity(const std::string& tag, uint32_t parent) {
  return GetSystem<TransformSystem>()->GetChildEntity(parent, tag);
}

size_t Scene::GetComponentSignaturePosition(size_t hash) {
  return component_signature_positions_.at(hash);
}

void Scene::Update(float ts) {
  for (auto& [name, system] : systems_) {
    system->OnUpdate(ts);
  }

  event_system_->DespatchEvents();  // clears event queue
}

}  // namespace engine
