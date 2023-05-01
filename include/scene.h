#ifndef ENGINE_INCLUDE_SCENE_H_
#define ENGINE_INCLUDE_SCENE_H_

#include <cassert>
#include <cstdint>
#include <map>
#include <typeinfo>

#include "ecs_system.h"
#include "event_system.h"

namespace engine {

class Application;

class Scene {
 public:
  Scene(Application* app);
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  ~Scene();

  void Update(float ts);

  Application* app() { return app_; }

  EventSystem* event_system() { return event_system_.get(); }

  /* ecs stuff */

  uint32_t CreateEntity(const std::string& tag, uint32_t parent = 0);

  uint32_t getEntity(const std::string& tag, uint32_t parent = 0);

  size_t GetComponentSignaturePosition(size_t hash);

  template <typename T>
  void registerComponent() {
    size_t hash = typeid(T).hash_code();
    assert(component_arrays_.contains(hash) == false &&
           "Registering component type more than once.");
    component_arrays_.emplace(hash, std::make_unique<ComponentArray<T>>());

    size_t signature_position = next_signature_position_;
    ++next_signature_position_;
    assert(signature_position < kMaxComponents &&
           "Registering too many components!");
    assert(component_signature_positions_.contains(hash) == false);
    component_signature_positions_.emplace(hash, signature_position);
  }

  template <typename T>
  T* GetComponent(uint32_t entity) {
    auto array = GetComponentArray<T>();
    return array->GetData(entity);
  }

  template <typename T>
  T* AddComponent(uint32_t entity) {
    size_t hash = typeid(T).hash_code();

    auto array = GetComponentArray<T>();
    array->InsertData(entity, T{});  // errors if entity already exists in array

    // set the component bit for this entity
    size_t signature_position = component_signature_positions_.at(hash);
    auto& signature_ref = signatures_.at(entity);
    signature_ref.set(signature_position);

    for (auto& [system_name, system] : systems_) {
      if (system->entities_.contains(entity)) continue;
      if ((system->signature_ & signature_ref) == system->signature_) {
        system->entities_.insert(entity);
        system->OnComponentInsert(entity);
      }
    }

    return array->GetData(entity);
  }

  template <typename T>
  void RegisterSystem() {
    size_t hash = typeid(T).hash_code();
    assert(systems_.find(hash) == systems_.end() &&
           "Registering system more than once.");
    systems_.emplace(hash, std::make_unique<T>(this));
  }

  template <typename T>
  T* GetSystem() {
    size_t hash = typeid(T).hash_code();
    auto it = systems_.find(hash);
    if (it == systems_.end()) {
      throw std::runtime_error("Cannot find ecs system.");
    }
    auto ptr = it->second.get();
    auto casted_ptr = dynamic_cast<T*>(ptr);
    assert(casted_ptr != nullptr);
    return casted_ptr;
  }

 private:
  Application* const app_;
  uint32_t next_entity_id_ = 1000;

  /* ecs stuff */

  size_t next_signature_position_ = 0;
  // maps component hashes to signature positions
  std::map<size_t, size_t> component_signature_positions_{};
  // maps entity ids to their signatures
  std::map<uint32_t, std::bitset<kMaxComponents>> signatures_{};
  // maps component hashes to their arrays
  std::map<size_t, std::unique_ptr<IComponentArray>> component_arrays_{};
  // maps system hashes to their class instantiations
  std::map<size_t, std::unique_ptr<System>> systems_{};

  template <typename T>
  ComponentArray<T>* GetComponentArray() {
    size_t hash = typeid(T).hash_code();
    auto it = component_arrays_.find(hash);
    if (it == component_arrays_.end()) {
      throw std::runtime_error("Cannot find component array.");
    }
    auto ptr = it->second.get();
    auto casted_ptr = dynamic_cast<ComponentArray<T>*>(ptr);
    assert(casted_ptr != nullptr);
    return casted_ptr;
  }

  std::unique_ptr<EventSystem> event_system_{};
};

}  // namespace engine

#endif