#ifndef ENGINE_DISABLE_PHYSICS

#include "physics.h"

#include <stdexcept>

#include <PxPhysicsAPI.h>

#include <PxConfig.h>

#include "log.h"

namespace engine {

class ErrorCallback : public physx::PxErrorCallback {
    void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) final
    {
        fprintf(stderr, "PhysX error:\n\tcode: %d\n\tmsg: %s\n\tfile: %s\n\tline: %d\n", static_cast<int>(code), message, file, line);
    }
};

class AllocatorCallback : public physx::PxAllocatorCallback {
    void* allocate(size_t size, const char* typeName, const char* filename, int line) override
    {
        void* ptr = malloc(size);
        if (!ptr) {
            throw std::runtime_error("PhysX allocation error, failed to allocate!");
        }
        return ptr;
    }
    void deallocate(void* ptr) override { free(ptr); }
};

struct PhysicsImpl {
    physx::PxFoundation* foundation{};
    ErrorCallback error_callback{};
    AllocatorCallback allocator_callback{};
    physx::PxPhysics* physics{};
};

// DEFINITIONS

PhysicsSceneConnection::PhysicsSceneConnection(int test) : m_test(test) {}

PhysicsSceneConnection::~PhysicsSceneConnection() {}

bool Physics::s_instanced = false;

Physics::Physics(const PhysicsInfo& info) : m_impl(std::make_unique<PhysicsImpl>())
{
    if (s_instanced) {
        throw std::runtime_error("Attempt to create another 'Physics' object");
    }
    else {
        s_instanced = true;
    }

    m_impl->foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_impl->allocator_callback, m_impl->error_callback);
    if (!m_impl->foundation) {
        throw std::runtime_error("Failed to create PhysX foundation");
    }

    m_impl->physics =
        PxCreatePhysics(PX_PHYSICS_VERSION, *m_impl->foundation, physx::PxTolerancesScale(info.default_length, info.default_speed), false, nullptr, nullptr);
    if (!m_impl->physics) {
        throw std::runtime_error("Failed to create PhysX physics!");
    }

    LOG_TRACE("Physics created!");
}

Physics::~Physics()
{
    PX_RELEASE(m_impl->physics);
    PX_RELEASE(m_impl->foundation);
}

} // namespace engine

#endif
