#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <bitset>
#include <map>
#include <set>
#include <vector>

#include "entity.h"

namespace engine {

class Scene; // forward-dec

constexpr size_t MAX_COMPONENTS = 10;

class IComponentArray {
public:
    virtual ~IComponentArray() = default;
};

template <typename T>
class ComponentArray : public IComponentArray {
private:
    std::vector<T> m_component_array{};

public:
    void insertData(Entity entity, T&& component)
    {
        if (m_component_array.size() < entity + 1) {
            m_component_array.resize(entity + 1);
        }
        // bounds checking here as not performance critical
        m_component_array.at(entity) = std::move(component);
    }

    void removeData(Entity entity)
    {
        (void)entity; // TODO
    }

    T* getData(Entity entity)
    {
        assert(entity < m_component_array.size());
        return &m_component_array[entity];
    }
};

class System {
public:
    Scene* const m_scene;
    std::bitset<MAX_COMPONENTS> m_signature;
    std::set<Entity> m_entities{}; // entities that contain the needed components

public:
    System(Scene* scene, std::set<size_t> required_component_hashes);
    System(const System&) = delete;

    virtual ~System() {}

    System& operator=(const System&) = delete;

    virtual void onUpdate(float ts) = 0;
    virtual void onComponentInsert(Entity) {}
    virtual void onComponentRemove(Entity) {}
};

} // namespace engine