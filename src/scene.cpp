#include "scene.h"

#include "components/transform.h"
#include "components/collider.h"
#include "components/custom.h"
#include "components/mesh_renderable.h"
#include "components/ui_renderable.h"
#include "systems/transform.h"
#include "systems/mesh_render_system.h"
#include "systems/ui_render_system.h"
#include "systems/collisions.h"
#include "systems/custom_behaviour.h"

namespace engine {

Scene::Scene(Application* app) : app_(app) {
  // event system
  event_system_ = std::make_unique<EventSystem>();

  // ecs configuration:

  RegisterComponent<TransformComponent>();
  RegisterComponent<ColliderComponent>();
  RegisterComponent<CustomComponent>();
  RegisterComponent<MeshRenderableComponent>();
  RegisterComponent<UIRenderableComponent>();

  // Order here matters:
  RegisterSystem<TransformSystem>();
  RegisterSystem<CollisionSystem>();
  RegisterSystem<CustomBehaviourSystem>();
  RegisterSystem<MeshRenderSystem>();
  RegisterSystem<UIRenderSystem>();
}

Scene::~Scene() {}

Entity Scene::CreateEntity(const std::string& tag, Entity parent,
                           const glm::vec3& pos, const glm::quat& rot,
                           const glm::vec3& scl) {
  Entity id = next_entity_id_++;

  signatures_.emplace(id, std::bitset<kMaxComponents>{});

  auto t = AddComponent<TransformComponent>(id);

  t->position = pos;
  t->rotation = rot;
  t->scale = scl;

  t->tag = tag;
  t->parent = parent;

  return id;
}

Entity Scene::GetEntity(const std::string& tag, Entity parent) {
  return GetSystem<TransformSystem>()->GetChildEntity(parent, tag);
}

size_t Scene::GetComponentSignaturePosition(size_t hash) {
  return component_signature_positions_.at(hash);
}

void Scene::Update(float ts) {
  for (auto& [name, system] : ecs_systems_) {
    system->OnUpdate(ts);
  }

  event_system_->DespatchEvents();  // clears event queue
}

}  // namespace engine
