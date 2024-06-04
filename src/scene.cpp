#include "scene.h"

#include "component_transform.h"
#include "component_collider.h"
#include "component_custom.h"
#include "component_mesh.h"
#include "system_transform.h"
#include "system_mesh_render.h"
#include "system_collisions.h"
#include "system_custom_behaviour.h"

namespace engine {

Scene::Scene(Application* app) : app_(app) {
  // event system
  event_system_ = std::make_unique<EventSystem>();

  // ecs configuration:

  RegisterComponent<TransformComponent>();
  RegisterComponent<ColliderComponent>();
  RegisterComponent<CustomComponent>();
  RegisterComponent<MeshRenderableComponent>();

  // Order here matters:
  RegisterSystem<CustomBehaviourSystem>(); // potentially modifies transforms
  RegisterSystem<TransformSystem>();
  RegisterSystem<CollisionSystem>(); // depends on transformed world matrix
  RegisterSystem<MeshRenderSystem>(); // depends on transformed world matrix
}

Scene::~Scene() {}

Entity Scene::CreateEntity(const std::string& tag, Entity parent,
                           const glm::vec3& pos, const glm::quat& rot,
                           const glm::vec3& scl) {
  Entity id = next_entity_id_++;

  signatures_.emplace(id, std::bitset<MAX_COMPONENTS>{});

  auto t = AddComponent<TransformComponent>(id);

  t->position = pos;
  t->rotation = rot;
  t->scale = scl;

  t->tag = tag;
  t->parent = parent;
  t->is_static = false;

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
    system->onUpdate(ts);
  }

  event_system_->DespatchEvents();  // clears event queue
}

}  // namespace engine
