#pragma once

#include <functional>
#include <memory>

#include "entity.h"

namespace engine {

class CustomBehaviourSystem; // foward-dec
class Scene; // forward-dec

class ComponentCustomImpl {
    friend CustomBehaviourSystem; // to set scene and entity on init

protected:
    Scene* m_scene = nullptr;
    Entity m_entity = 0u;

public:
    ComponentCustomImpl() = default;
    ComponentCustomImpl(const ComponentCustomImpl&) = delete;

    virtual ~ComponentCustomImpl() = 0;

    virtual void init() {}
    virtual void update([[maybe_unused]] float dt) {}
};

inline ComponentCustomImpl::~ComponentCustomImpl() {}

struct CustomComponent {
    std::unique_ptr<ComponentCustomImpl> impl;
};

} // namespace engine
