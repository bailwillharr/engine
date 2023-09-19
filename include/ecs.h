#ifndef ENGINE_INCLUDE_ECS_H_
#define ENGINE_INCLUDE_ECS_H_

#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include <set>

namespace engine {

class Scene;

using Entity = uint32_t; // ECS entity

constexpr size_t kMaxComponents = 10;

class IComponentArray {
 public:
  virtual ~IComponentArray() = default;
};

template <typename T>
class ComponentArray : public IComponentArray {
 public:
  void InsertData(Entity entity, T component) {
    if (component_array_.size() < entity + 1) {
      component_array_.resize(entity + 1);
    }
    // bounds checking here as not performance critical
    component_array_.at(entity) = component;
  }

  void DeleteData(Entity entity) {
    (void)entity;  // TODO
  }

  T* GetData(Entity entity) {
    assert(entity < component_array_.size());
    return &component_array_[entity];
  }

 private:
  std::vector<T> component_array_{};
};

class System {
 public:
  System(Scene* scene, std::set<size_t> required_component_hashes);
  virtual ~System() {}
  System(const System&) = delete;
  System& operator=(const System&) = delete;

  virtual void OnUpdate(float ts) = 0;

  virtual void OnComponentInsert(Entity) {}
  virtual void OnComponentRemove(Entity) {}

  Scene* const scene_;

  std::bitset<kMaxComponents> signature_;

  // entities that contain the needed components
  std::set<Entity> entities_{};
};

}  // namespace engine

#endif