#pragma once

#include <memory>

namespace engine {

struct PhysicsImpl;

struct PhysicsInfo {
    float default_length; // typical object length (1 m)
    float default_speed; // typical speed after one second of falling (9.8 m/s)
};

/* 
* Owned by Physics class, reference is stored in each Scene SystemPhysics object.
* This lets the Physics System access collision/physics info for its scene
*/
class PhysicsSceneConnection {
    int m_test;

public:
    PhysicsSceneConnection() = delete;
    explicit PhysicsSceneConnection(int test);
    PhysicsSceneConnection(const PhysicsSceneConnection&) = delete;
    PhysicsSceneConnection(PhysicsSceneConnection&&) = delete;

    ~PhysicsSceneConnection();

    PhysicsSceneConnection& operator=(const PhysicsSceneConnection&) = delete;
    PhysicsSceneConnection& operator=(PhysicsSceneConnection&&) = delete;
};

/*
* PhysX wrapper for the game engine.
* Does not expose the PhysX API.
* As an application-level class, this must support multiple scenes.
*/
class Physics {
    static bool s_instanced;
    std::unique_ptr<PhysicsImpl> m_impl;

public:
    Physics() = delete;
    Physics(const PhysicsInfo& info);
    Physics(const Physics&) = delete;
    Physics(Physics&&) = delete;

    ~Physics();

    Physics& operator=(const Physics&) = delete;
    Physics& operator=(Physics&&) = delete;


};

} // namespace engine