#pragma once

#include <cassert>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <typeinfo>

#include <glm/vec3.hpp>
#include <glm/ext/quaternion_float.hpp>

#include "ecs.h"
#include "event_system.h"
#include "component_transform.h"

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

    Entity CreateEntity(const std::string& tag, Entity parent = 0, const glm::vec3& pos = glm::vec3{0.0f, 0.0f, 0.0f},
                        const glm::quat& rot = glm::quat{1.0f, 0.0f, 0.0f, 0.0f}, const glm::vec3& scl = glm::vec3{1.0f, 1.0f, 1.0f});

    Entity GetEntity(const std::string& tag, Entity parent = 0);

    size_t GetComponentSignaturePosition(size_t hash);

    template <typename T>
    void RegisterComponent()
    {
        size_t hash = typeid(T).hash_code();
        assert(component_arrays_.contains(hash) == false && "Registering component type more than once.");
        component_arrays_.emplace(hash, std::make_unique<ComponentArray<T>>());

        size_t signature_position = next_signature_position_;
        ++next_signature_position_;
        assert(signature_position < kMaxComponents && "Registering too many components!");
        assert(component_signature_positions_.contains(hash) == false);
        component_signature_positions_.emplace(hash, signature_position);
    }

    template <typename T>
    T* GetComponent(Entity entity)
    {
        // check if component exists on entity:
        size_t hash = typeid(T).hash_code();
        size_t signature_position = component_signature_positions_.at(hash);
        const auto& entity_signature = signatures_.at(entity);
        if (entity_signature.test(signature_position) == false) {
            return nullptr;
        }

        auto array = GetComponentArray<T>();
        return array->GetData(entity);
    }

    // because GetComponent<Transformzzzzzz takes too long
    TransformComponent* GetTransform(Entity entity) { return GetComponent<TransformComponent>(entity); }

    glm::vec3& GetPosition(Entity entity) { return GetTransform(entity)->position; }

    glm::quat& GetRotation(Entity entity) { return GetTransform(entity)->rotation; }

    glm::vec3& GetScale(Entity entity) { return GetTransform(entity)->scale; }

    template <typename T>
    T* AddComponent(Entity entity, const T& comp = T{})
    {
        size_t hash = typeid(T).hash_code();

        auto array = GetComponentArray<T>();
        array->InsertData(entity, comp); // errors if entity already exists in array

        // set the component bit for this entity
        size_t signature_position = component_signature_positions_.at(hash);
        auto& signature_ref = signatures_.at(entity);
        signature_ref.set(signature_position);

        for (auto& [system_hash, system] : ecs_systems_) {
            if (system->entities_.contains(entity)) continue;
            if ((system->signature_ & signature_ref) == system->signature_) {
                system->entities_.insert(entity);
                system->OnComponentInsert(entity);
            }
        }

        return array->GetData(entity);
    }

    template <typename T>
    void RegisterSystem()
    {
        size_t hash = typeid(T).hash_code();
        ecs_systems_.emplace_back(hash, std::make_unique<T>(this));
    }

    /* Pushes old systems starting at 'index' along by 1 */
    template <typename T>
    void RegisterSystemAtIndex(size_t index)
    {
        size_t hash = typeid(T).hash_code();
        ecs_systems_.emplace(ecs_systems_.begin() + index, hash, std::make_unique<T>(this));
    }

    template <typename T>
    T* GetSystem()
    {
        size_t hash = typeid(T).hash_code();
        System* found_system = nullptr;
        for (auto& [system_hash, system] : ecs_systems_) {
            if (hash == system_hash) found_system = system.get();
        }
        if (found_system == nullptr) {
            throw std::runtime_error("Unable to find system");
        }
        T* casted_ptr = dynamic_cast<T*>(found_system);
        if (casted_ptr == nullptr) {
            throw std::runtime_error("Failed to cast system pointer!");
        }
        return casted_ptr;
    }

   private:
    Application* const app_;

   public:
    Entity next_entity_id_ = 1; // 0 is not a valid entity
   private:
    uint64_t framecount_ = 0;

    /* ecs stuff */

    size_t next_signature_position_ = 0;
    // maps component hashes to signature positions
    std::unordered_map<size_t, size_t> component_signature_positions_{};
    // maps entity ids to their signatures
    std::unordered_map<Entity, std::bitset<kMaxComponents>> signatures_{};
    // maps component hashes to their arrays
    std::unordered_map<size_t, std::unique_ptr<IComponentArray>> component_arrays_{};

    // hashes and associated systems
    std::vector<std::pair<size_t, std::unique_ptr<System>>> ecs_systems_{};

    template <typename T>
    ComponentArray<T>* GetComponentArray()
    {
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

} // namespace engine