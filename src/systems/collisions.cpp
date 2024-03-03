#include "systems/collisions.h"

#include "components/transform.h"
#include "components/collider.h"
#include "scene.h"

#include "log.h"

#include <array>
#include <iostream>

namespace engine {

static glm::vec3 GetBoxCentroid(const AABB& box)
{
    glm::vec3 v{};
    v.x = (box.min.x + box.max.x) * 0.5f;
    v.y = (box.min.y + box.max.y) * 0.5f;
    v.z = (box.min.z + box.max.z) * 0.5f;
    return v;
}

static float GetBoxArea(const AABB& box)
{
    float front_back = fabsf(box.max.x - box.min.x) * fabs(box.max.y - box.min.y) * 2.0f;
    float left_right = fabsf(box.max.z - box.min.z) * fabs(box.max.y - box.min.y) * 2.0f;
    float top_bottom = fabsf(box.max.x - box.min.x) * fabs(box.max.z - box.min.z) * 2.0f;
    return front_back + left_right + top_bottom;
}

// returns true on hit and tmin
static std::pair<bool, float> RayBoxIntersection(const Ray& ray, const AABB& box, float t)
{
    // Thank you https://tavianator.com/cgit/dimension.git/tree/libdimension/bvh/bvh.c
    glm::vec3 n_inv{1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z};

    float tx1 = (box.min.x - ray.origin.x) * n_inv.x;
    float tx2 = (box.max.x - ray.origin.x) * n_inv.x;

    float tmin = fminf(tx1, tx2);
    float tmax = fmaxf(tx1, tx2);

    float ty1 = (box.min.y - ray.origin.y) * n_inv.y;
    float ty2 = (box.max.y - ray.origin.y) * n_inv.y;

    tmin = fmaxf(tmin, fminf(ty1, ty2));
    tmax = fminf(tmax, fmaxf(ty1, ty2));

    float tz1 = (box.min.z - ray.origin.z) * n_inv.z;
    float tz2 = (box.max.z - ray.origin.z) * n_inv.z;

    tmin = fmaxf(tmin, fminf(tz1, tz2));
    tmax = fminf(tmax, fmaxf(tz1, tz2));

    return std::make_pair((tmax >= fmaxf(0.0, tmin) && tmin < t), tmin);
}

// class methods

CollisionSystem::CollisionSystem(Scene* scene) : System(scene, {typeid(TransformComponent).hash_code(), typeid(ColliderComponent).hash_code()}) {}

void CollisionSystem::OnComponentInsert(Entity entity) { ++colliders_size_now_; }

void CollisionSystem::OnUpdate(float ts)
{
    (void)ts;

    if (colliders_size_last_update_ != colliders_size_now_) {

        std::vector<PrimitiveInfo> prims{};
        prims.reserve(entities_.size());
        for (Entity entity : entities_) {
            const auto t = scene_->GetComponent<TransformComponent>(entity);
            const auto c = scene_->GetComponent<ColliderComponent>(entity);

            AABB transformed_box{};
            transformed_box.min = t->world_matrix * glm::vec4(c->aabb.min, 1.0f);
            transformed_box.max = t->world_matrix * glm::vec4(c->aabb.max, 1.0f);
            // if a mesh is not rotated a multiple of 90 degrees, the box will not fit the mesh
            // TODO fix it
            prims.emplace_back(c->aabb, entity);
        }

        bvh_.clear();
        bvh_.reserve(entities_.size() * 3 / 2); // from testing, bvh is usually 20% larger than number of objects
        BuildNode(prims, bvh_);

        LOG_INFO("BUILT BVH!");

        colliders_size_last_update_ = colliders_size_now_;
    }
}

Raycast CollisionSystem::GetRaycast(Ray ray)
{
    Raycast res{};

    ray.direction = glm::normalize(ray.direction);
    
    res.hit = RaycastTreeNode(ray, bvh_.back(), res.location, res.distance, res.hit_entity);

    return res;
}

CollisionSystem::PrimitiveInfo::PrimitiveInfo(const AABB& aabb, Entity entity_idx) : entity(entity_idx), box(aabb), centroid(GetBoxCentroid(aabb)) {}

// returns the index of the node just added
// 'prims' will be sorted randomly and should be treated as garbage
int CollisionSystem::BuildNode(std::vector<PrimitiveInfo>& prims, std::vector<BiTreeNode>& tree_nodes)
{

    if (prims.size() == 0) throw std::runtime_error("Cannot build BVH with no primitives!");

    std::array<std::function<bool(const PrimitiveInfo&, const PrimitiveInfo&)>, 3> centroid_tests{
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.centroid.x < p2.centroid.x); },
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.centroid.y < p2.centroid.y); },
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.centroid.z < p2.centroid.z); }};
    std::array<std::function<bool(const PrimitiveInfo&, const PrimitiveInfo&)>, 3> box_min_tests{
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.box.min.x < p2.box.min.x); },
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.box.min.y < p2.box.min.y); },
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.box.min.z < p2.box.min.z); }};
    std::array<std::function<bool(const PrimitiveInfo&, const PrimitiveInfo&)>, 3> box_max_tests{
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.box.max.x < p2.box.max.x); },
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.box.max.y < p2.box.max.y); },
        [](const PrimitiveInfo& p1, const PrimitiveInfo& p2) -> bool { return (p1.box.max.z < p2.box.max.z); }};

    BiTreeNode node{};

    if (prims.size() > 2) {
        AABB optimal_box1{}, optimal_box2{};
        std::vector<PrimitiveInfo> sorted_prims;
        sorted_prims.reserve(prims.size());
        int sorted_prims_split_index = 0;

        float sah = std::numeric_limits<float>().infinity(); // surface area heuristic

        // try along each axis
        for (int axis = 0; axis < 3; axis++) {

            int other_axis1 = (axis + 1) % 3;
            int other_axis2 = (axis + 2) % 3;

            std::sort(prims.begin(), prims.end(), centroid_tests[axis]);

            // std::cout << "Sorting on axis " << axis << "...\n";
            for (const auto& p : prims) {
                // std::cout << p.centroid.x << "\t" << p.centroid.y << "\t" << p.centroid.z << "\n";
            }
            // std::cout << "\n\n\n";

            // break;

            // split the boxes
            for (int i = 0; i < prims.size() - 1; i++) {

                float box1_main_min =
                    reinterpret_cast<const float*>(&(std::min_element(prims.begin(), prims.begin() + i + 1, box_min_tests[axis])->box.min))[axis];
                float box1_main_max =
                    reinterpret_cast<const float*>(&(std::max_element(prims.begin(), prims.begin() + i + 1, box_max_tests[axis])->box.max))[axis];
                float box2_main_min =
                    reinterpret_cast<const float*>(&(std::min_element(prims.begin() + i + 1, prims.end(), box_min_tests[axis])->box.min))[axis];
                float box2_main_max =
                    reinterpret_cast<const float*>(&(std::max_element(prims.begin() + i + 1, prims.end(), box_max_tests[axis])->box.max))[axis];

                float box1_min1 =
                    reinterpret_cast<const float*>(&(std::min_element(prims.begin(), prims.begin() + i + 1, box_min_tests[other_axis1])->box.min))[other_axis1];
                float box1_max1 =
                    reinterpret_cast<const float*>(&(std::max_element(prims.begin(), prims.begin() + i + 1, box_max_tests[other_axis1])->box.max))[other_axis1];
                float box1_min2 =
                    reinterpret_cast<const float*>(&(std::min_element(prims.begin(), prims.begin() + i + 1, box_min_tests[other_axis2])->box.min))[other_axis2];
                float box1_max2 =
                    reinterpret_cast<const float*>(&(std::max_element(prims.begin(), prims.begin() + i + 1, box_max_tests[other_axis2])->box.max))[other_axis2];

                float box2_min1 =
                    reinterpret_cast<const float*>(&(std::min_element(prims.begin() + i + 1, prims.end(), box_min_tests[other_axis1])->box.min))[other_axis1];
                float box2_max1 =
                    reinterpret_cast<const float*>(&(std::max_element(prims.begin() + i + 1, prims.end(), box_max_tests[other_axis1])->box.max))[other_axis1];
                float box2_min2 =
                    reinterpret_cast<const float*>(&(std::min_element(prims.begin() + i + 1, prims.end(), box_min_tests[other_axis2])->box.min))[other_axis2];
                float box2_max2 =
                    reinterpret_cast<const float*>(&(std::max_element(prims.begin() + i + 1, prims.end(), box_max_tests[other_axis2])->box.max))[other_axis2];

                AABB box1{}, box2{};
                switch (axis) {
                    case 0:
                        // x
                        box1.min.x = box1_main_min;
                        box1.min.y = box1_min1;
                        box1.min.z = box1_min2;
                        box1.max.x = box1_main_max;
                        box1.max.y = box1_max1;
                        box1.max.z = box1_max2;
                        box2.min.x = box2_main_min;
                        box2.min.y = box2_min1;
                        box2.min.z = box2_min2;
                        box2.max.x = box2_main_max;
                        box2.max.y = box2_max1;
                        box2.max.z = box2_max2;
                        break;
                    case 1:
                        // y
                        box1.min.x = box1_min2;
                        box1.min.y = box1_main_min;
                        box1.min.z = box1_min1;
                        box1.max.x = box1_max2;
                        box1.max.y = box1_main_max;
                        box1.max.z = box1_max1;
                        box2.min.x = box2_min2;
                        box2.min.y = box2_main_min;
                        box2.min.z = box2_min1;
                        box2.max.x = box2_max2;
                        box2.max.y = box2_main_max;
                        box2.max.z = box2_max1;
                        break;
                    case 2:
                        // z
                        box1.min.x = box1_min1;
                        box1.min.y = box1_min2;
                        box1.min.z = box1_main_min;
                        box1.max.x = box1_max1;
                        box1.max.y = box1_max2;
                        box1.max.z = box1_main_max;
                        box2.min.x = box2_min1;
                        box2.min.y = box2_min2;
                        box2.min.z = box2_main_min;
                        box2.max.x = box2_max1;
                        box2.max.y = box2_max2;
                        box2.max.z = box2_main_max;
                        break;
                }
                const float combined_surface_area = (GetBoxArea(box1) * (i + 1)) + (GetBoxArea(box2) * (prims.size() - (i + 1)));
                if (combined_surface_area < sah) {
                    sah = combined_surface_area;
                    optimal_box1 = box1;
                    optimal_box2 = box2;
                    sorted_prims = prims; // VECTOR COPY!
                    sorted_prims_split_index = i;
                    std::cout << "picking new boxes, axis: " << axis << " i: " << i << " sah: " << combined_surface_area << "\n";
                }
            }
        }

        // now we have:
        // a vector of sorted prims
        // the index where these primitives are split
        // the two bounding boxes
        node.box1 = optimal_box1;
        node.box2 = optimal_box2;
        std::vector<PrimitiveInfo> prims1(sorted_prims.begin(), sorted_prims.begin() + sorted_prims_split_index + 1);
        int index1 = BuildNode(prims1, tree_nodes);
        std::vector<PrimitiveInfo> prims2(sorted_prims.begin() + sorted_prims_split_index + 1, sorted_prims.end());
        int index2 = BuildNode(prims2, tree_nodes);
        node.index1 = index1;
        node.index2 = index2;
        node.type1 = BiTreeNode::Type::BoundingVolume;
        node.type2 = BiTreeNode::Type::BoundingVolume;
    }
    else {
        if (prims.size() == 2) {
            node.box1 = prims[0].box;
            node.box2 = prims[1].box;
            node.index1 = prims[0].entity;
            node.index2 = prims[1].entity;
            node.type1 = BiTreeNode::Type::Entity;
            node.type2 = BiTreeNode::Type::Entity;
        }
        else {
            // prims.size() == 1
            node.box1 = prims[0].box;
            node.index1 = prims[0].entity;
            node.type1 = BiTreeNode::Type::Entity;
            node.type2 = BiTreeNode::Type::Empty;
        }
    }

    tree_nodes.push_back(node);
    int node_index = static_cast<int>(tree_nodes.size() - 1);

    return node_index;
}

// returns true on ray hit
bool CollisionSystem::RaycastTreeNode(const Ray& ray, const BiTreeNode& node, glm::vec3& location, float& t, Entity& object_index)
{

    using Type = BiTreeNode::Type;

    constexpr float T = 1000.0f;

    bool is_hit1 = false;
    bool is_hit2 = false;
    float t1 = std::numeric_limits<float>::infinity();
    float t2 = std::numeric_limits<float>::infinity();

    if (node.type1 != Type::Empty) {
        auto [is_hit, t] = RayBoxIntersection(ray, node.box1, T);
        is_hit1 = is_hit;
        t1 = t;
    }
    if (node.type2 != Type::Empty) {
        auto [is_hit, t] = RayBoxIntersection(ray, node.box2, T);
        is_hit2 = is_hit;
        t2 = t;
    }

    // possible outcomes:

    // neither hit
    if (!is_hit1 && !is_hit2) return false;

    // when both hit
    if (is_hit1 && is_hit2) {
        // if 1 is a BV and 2 a gameobject, see if gameobject is in front
        if (node.type1 == Type::BoundingVolume && node.type2 == Type::Entity) {
            if (t2 < t1) {
                location = (ray.direction * t2) + ray.origin;
                t = t2;
                object_index = node.index2;
                return true;
            }
            else
                return RaycastTreeNode(ray, bvh_.at(node.index1), location, t, object_index);
        }

        // if 1 is a gameobject and 2 a BV, see if gameobject is in front
        if (node.type1 == Type::Entity && node.type2 == Type::BoundingVolume) {
            if (t1 < t2) {
                location = (ray.direction * t1) + ray.origin;
                t = t1;
                object_index = node.index1;
                return true;
            }
            else
                return RaycastTreeNode(ray, bvh_.at(node.index2), location, t, object_index);
        }

        // if 1 is a BV and 2 is a BV
        if (node.type1 == Type::BoundingVolume && node.type2 == Type::BoundingVolume) {
            float node1_t{};
            glm::vec3 location1{};
            Entity object_index1{};
            bool node1_intersects = RaycastTreeNode(ray, bvh_.at(node.index1), location1, node1_t, object_index1);
            float node2_t{};
            glm::vec3 location2{};
            Entity object_index2;
            bool node2_intersects = RaycastTreeNode(ray, bvh_.at(node.index2), location2, node2_t, object_index2);
            if (node1_intersects && node2_intersects) {
                if (node1_t < node2_t) {
                    location = location1;
                    t = node1_t;
                    object_index = object_index1;
                    return true;
                }
                else {
                    location = location2;
                    t = node2_t;
                    object_index = object_index1;
                    return true;
                }
            }
            else if (node1_intersects) {
                t = node1_t;
                location = location1;
                object_index = object_index1;
                return true;
            }
            else if (node2_intersects) {
                t = node2_t;
                location = location2;
                object_index = object_index2;
                return true;
            }
            else {
                return false;
            }
        }

        // if 1 is a gameobject and 2 is a gameobject
        if (node.type1 == Type::Entity && node.type2 == Type::Entity) {
            if (t1 < t2) {
                location = (ray.direction * t1) + ray.origin;
                t = t1;
                object_index = node.index1;
            }
            else {
                location = (ray.direction * t2) + ray.origin;
                t = t2;
                object_index = node.index2;
            }
            return true;
        }
    }

    // only 1 hits
    if (is_hit1) {
        switch (node.type1) {
            case Type::BoundingVolume:
                return RaycastTreeNode(ray, bvh_.at(node.index1), location, t, object_index);
            case Type::Entity:
                location = (ray.direction * t1) + ray.origin;
                t = t1;
                object_index = node.index1;
                return true;
        }
    }

    // only 2 hits
    if (is_hit2) {
        switch (node.type2) {
            case Type::BoundingVolume:
                return RaycastTreeNode(ray, bvh_.at(node.index2), location, t, object_index);
            case Type::Entity:
                location = (ray.direction * t2) + ray.origin;
                t = t2;
                object_index = node.index2;
                return true;
        }
    }
}

} // namespace engine
