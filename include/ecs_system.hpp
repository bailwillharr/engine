#ifndef ENGINE_INCLUDE_ECS_SYSTEM_H_
#define ENGINE_INCLUDE_ECS_SYSTEM_H_

#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>

namespace engine {

class Scene;

constexpr size_t kMaxComponents = 64;

class IComponentArray {
 public:
  virtual ~IComponentArray() = default;
};

template <typename T>
class ComponentArray : public IComponentArray {
 public:
  void InsertData(uint32_t entity, T component) {
    assert(component_array_.find(entity) == component_array_.end() &&
           "Adding component which already exists to entity");
    component_array_.emplace(entity, component);
  }

  void DeleteData(uint32_t entity) { component_array_.erase(entity); }

  T* GetData(uint32_t entity) {
    if (component_array_.contains(entity)) {
      return &(component_array_.at(entity));
    } else {
      return nullptr;
    }
  }

 private:
  std::map<uint32_t, T> component_array_{};
};

class System {
 public:
  System(Scene* scene, std::set<size_t> required_component_hashes);
  virtual ~System() {}
  System(const System&) = delete;
  System& operator=(const System&) = delete;

  virtual void OnUpdate(float ts) = 0;

  virtual void OnComponentInsert(uint32_t) {}
  virtual void OnComponentRemove(uint32_t) {}

  Scene* const scene_;

  std::bitset<kMaxComponents> signature_;

  // entities that contain the needed components
  std::set<uint32_t> entities_{};
};

}  // namespace engine

#endif