#ifndef ENGINE_INCLUDE_SYSTEMS_COLLISIONS_H_
#define ENGINE_INCLUDE_SYSTEMS_COLLISIONS_H_

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

#include "components/collider.h"
#include "ecs.h"

namespace engine {

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct Raycast {
    glm::vec3 location;
    glm::vec3 normal;
    Entity hit_entity; // broken
    float distance;
    bool hit;
};

enum class AABBSide { Left, Right, Bottom, Top, Front, Back };

class CollisionSystem : public System {
   public:
    CollisionSystem(Scene* scene);

    void OnComponentInsert(Entity entity) override;

    void OnUpdate(float ts) override;

    Raycast GetRaycast(Ray ray);

   public:
    // one node of the BVH
    struct BiTreeNode {
        enum class Type : uint8_t { BoundingVolume, Entity, Empty };
        AABB box1;
        AABB box2;
        uint32_t index1; // index for either tree entry or entity, depending on type
        uint32_t index2;
        Type type1;
        Type type2;
    }; // 60 bytes with alignment of 4 allows for tight packing

    // an array of these is used to build the BVH
    struct PrimitiveInfo {
        Entity entity;
        AABB box;
        glm::vec3 centroid;
        PrimitiveInfo(const AABB& aabb, Entity entity_idx);
    };

    std::vector<BiTreeNode> bvh_{};

   private:
    size_t colliders_size_last_update_ = 0;
    size_t colliders_size_now_ = 0;

    struct RaycastTreeNodeResult {
        glm::vec3 location;
        Entity object_index;
        AABBSide side;
        float t;
        bool hit; // if this is false, all other values are undefined
    };
    RaycastTreeNodeResult RaycastTreeNode(const Ray& ray, const BiTreeNode& node);

    static int BuildNode(std::vector<PrimitiveInfo>& prims, std::vector<BiTreeNode>& tree_nodes);
};

} // namespace engine

#endif