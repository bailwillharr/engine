#include "system_collisions.h"

#include "component_transform.h"
#include "component_collider.h"
#include "scene.h"

#include "log.h"

#include <array>
#include <iostream>

namespace engine {

static AABB transformBox(const AABB& box, const glm::mat4& matrix)
{
    const std::array<glm::vec3, 8> points{glm::vec3{box.min.x, box.min.y, box.min.z}, glm::vec3{box.min.x, box.min.y, box.max.z},
                                          glm::vec3{box.min.x, box.max.y, box.min.z}, glm::vec3{box.min.x, box.max.y, box.max.z},
                                          glm::vec3{box.max.x, box.min.y, box.min.z}, glm::vec3{box.max.x, box.min.y, box.max.z},
                                          glm::vec3{box.max.x, box.max.y, box.min.z}, glm::vec3{box.max.x, box.max.y, box.max.z}};

    AABB new_box{};
    new_box.min.x = std::numeric_limits<float>::infinity();
    new_box.min.y = new_box.min.x;
    new_box.min.z = new_box.min.x;
    new_box.max.x = -std::numeric_limits<float>::infinity();
    new_box.max.y = new_box.max.x;
    new_box.max.z = new_box.max.x;

    // transform points of AABB to new positions and adjust min and max accordingly
    for (glm::vec3 point : points) {
        point = matrix * glm::vec4(point, 1.0f);
        new_box.min.x = fminf(new_box.min.x, point.x);
        new_box.min.y = fminf(new_box.min.y, point.y);
        new_box.min.z = fminf(new_box.min.z, point.z);
        new_box.max.x = fmaxf(new_box.max.x, point.x);
        new_box.max.y = fmaxf(new_box.max.y, point.y);
        new_box.max.z = fmaxf(new_box.max.z, point.z);
    }

    return new_box;
}

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
// also modifies 'side' reference
static std::pair<bool, float> RayBoxIntersection(const Ray& ray, const AABB& box, float t, AABBSide& side)
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

    side = AABBSide::Left;
    if (tmin == tx2)
        side = AABBSide::Right;
    else if (tmin == tx1)
        side = AABBSide::Left;
    else if (tmin == ty2)
        side = AABBSide::Back;
    else if (tmin == ty1)
        side = AABBSide::Front;
    else if (tmin == tz2)
        side = AABBSide::Top;
    else if (tmin == tz1)
        side = AABBSide::Bottom;

    return std::make_pair((tmax >= fmaxf(0.0, tmin) && tmin < t), tmin);
}

// class methods

CollisionSystem::CollisionSystem(Scene* scene) : System(scene, {typeid(TransformComponent).hash_code(), typeid(ColliderComponent).hash_code()}) {}

void CollisionSystem::onComponentInsert(Entity entity)
{
    (void)entity;
    ++colliders_size_now_;
}

void CollisionSystem::onUpdate(float ts)
{
    (void)ts;

    // TODO: the bvh will not update if an equal number of colliders are removed and added.
    // This is not a problem yet, as entitites cannot be removed from the scene.
    if (colliders_size_last_update_ != colliders_size_now_) {

        std::vector<PrimitiveInfo> prims{};
        prims.reserve(m_entities.size());
        for (Entity entity : m_entities) {
            const auto t = m_scene->GetComponent<TransformComponent>(entity);
            const auto c = m_scene->GetComponent<ColliderComponent>(entity);

            const AABB box = transformBox(c->aabb, t->world_matrix);

            prims.emplace_back(box, entity);
        }

        bvh_.clear();
        bvh_.reserve(m_entities.size() * 3 / 2); // from testing, bvh is usually 20% larger than number of objects
        BuildNode(prims, bvh_);

#ifndef NDEBUG
        // check AABB mins and maxes are the correct order
        for (const auto& node : bvh_) {
            if (node.type1 != BiTreeNode::Type::Empty) {
                if (node.box1.max.x < node.box1.min.x) abort();
                if (node.box1.max.y < node.box1.min.y) abort();
                if (node.box1.max.z < node.box1.min.z) abort();
            }
            if (node.type2 != BiTreeNode::Type::Empty) {
                if (node.box2.max.x < node.box2.min.x) abort();
                if (node.box2.max.y < node.box2.min.y) abort();
                if (node.box2.max.z < node.box2.min.z) abort();
            }
        }
#endif

        LOG_DEBUG("BUILT BVH!");

        colliders_size_last_update_ = colliders_size_now_;
    }
}

Raycast CollisionSystem::GetRaycast(Ray ray)
{
    ray.direction = glm::normalize(ray.direction);

    Raycast res{};
    res.hit = false;

    if (!bvh_.empty()) {
        const RaycastTreeNodeResult tree_node_cast_res = RaycastTreeNode(ray, bvh_.back());
        if (tree_node_cast_res.hit) {
            res.hit = true;
            res.distance = tree_node_cast_res.t;
            res.location = tree_node_cast_res.location;
            res.hit_entity = tree_node_cast_res.object_index;
            // find normal
            switch (tree_node_cast_res.side) {
                case AABBSide::Left:
                    res.normal = glm::vec3{-1.0, 0.0f, 0.0f};
                    break;
                case AABBSide::Right:
                    res.normal = glm::vec3{1.0, 0.0f, 0.0f};
                    break;
                case AABBSide::Bottom:
                    res.normal = glm::vec3{0.0, 0.0f, -1.0f};
                    break;
                case AABBSide::Top:
                    res.normal = glm::vec3{0.0, 0.0f, 1.0f};
                    break;
                case AABBSide::Front:
                    res.normal = glm::vec3{0.0, -1.0f, 0.0f};
                    break;
                case AABBSide::Back:
                    res.normal = glm::vec3{0.0, 1.0f, 0.0f};
                    break;
            }
        }
    }

    return res;
}

CollisionSystem::PrimitiveInfo::PrimitiveInfo(const AABB& aabb, Entity entity_idx) : entity(entity_idx), box(aabb), centroid(GetBoxCentroid(aabb)) {}

// returns the index of the node just added
// 'prims' will be sorted randomly and should be treated as garbage
int CollisionSystem::BuildNode(std::vector<PrimitiveInfo>& prims, std::vector<BiTreeNode>& tree_nodes)
{

    if (prims.size() == 0) abort();

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
            // for (const auto& p : prims) {
            // std::cout << p.centroid.x << "\t" << p.centroid.y << "\t" << p.centroid.z << "\n";
            //}
            // std::cout << "\n\n\n";

            // break;

            // split the boxes
            for (std::size_t i = 0; i < prims.size() - 1; i++) {

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
                const float combined_surface_area = (GetBoxArea(box1) * (i + 1)) + (GetBoxArea(box2) * (static_cast<int>(prims.size()) - (i + 1)));
                if (combined_surface_area < sah) {
                    sah = combined_surface_area;
                    optimal_box1 = box1;
                    optimal_box2 = box2;
                    sorted_prims = prims; // VECTOR COPY!
                    sorted_prims_split_index = i;
                    // std::cout << "picking new boxes, axis: " << axis << " i: " << i << " sah: " << combined_surface_area << "\n";
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
CollisionSystem::RaycastTreeNodeResult CollisionSystem::RaycastTreeNode(const Ray& ray, const BiTreeNode& node)
{

    using Type = BiTreeNode::Type;

    constexpr float T = 1000.0f;

    RaycastTreeNodeResult res{};

    bool is_hit1 = false;
    bool is_hit2 = false;
    float t1 = std::numeric_limits<float>::infinity();
    float t2 = std::numeric_limits<float>::infinity();

    AABBSide side1;
    AABBSide side2;

    if (node.type1 != Type::Empty) {
        auto [is_hit, t] = RayBoxIntersection(ray, node.box1, T, side1);
        is_hit1 = is_hit;
        t1 = t;
    }
    if (node.type2 != Type::Empty) {
        auto [is_hit, t] = RayBoxIntersection(ray, node.box2, T, side2);
        is_hit2 = is_hit;
        t2 = t;
    }

    // possible outcomes:

    if (!is_hit1 && !is_hit2) {
        res.hit = false;
        return res;
    }
    else if (is_hit1 && is_hit2) {
        // if 1 is a BV and 2 a gameobject, see if gameobject is in front
        if (node.type1 == Type::BoundingVolume && node.type2 == Type::Entity) {
            if (t2 < t1) {
                res.location = (ray.direction * t2) + ray.origin;
                res.t = t2;
                res.object_index = node.index2;
                res.side = side2;
                res.hit = true;
                return res;
            }
            else
                return RaycastTreeNode(ray, bvh_.at(node.index1));
        }

        // if 1 is a gameobject and 2 a BV, see if gameobject is in front
        if (node.type1 == Type::Entity && node.type2 == Type::BoundingVolume) {
            if (t1 < t2) {
                res.location = (ray.direction * t1) + ray.origin;
                res.t = t1;
                res.object_index = node.index1;
                res.side = side1;
                res.hit = true;
                return res;
            }
            else
                return RaycastTreeNode(ray, bvh_.at(node.index2));
        }

        // if 1 is a BV and 2 is a BV
        if (node.type1 == Type::BoundingVolume && node.type2 == Type::BoundingVolume) {
            auto node1_intersection = RaycastTreeNode(ray, bvh_.at(node.index1));
            auto node2_intersection = RaycastTreeNode(ray, bvh_.at(node.index2));
            if (node1_intersection.hit && node2_intersection.hit) {
                if (node1_intersection.t < node2_intersection.t) {
                    res.location = node1_intersection.location;
                    res.t = node1_intersection.t;
                    res.object_index = node1_intersection.object_index;
                    res.side = node1_intersection.side;
                    res.hit = true;
                    return res;
                }
                else {
                    res.location = node2_intersection.location;
                    res.t = node2_intersection.t;
                    res.object_index = node2_intersection.object_index;
                    res.side = node2_intersection.side;
                    res.hit = true;
                    return res;
                }
            }
            else if (node1_intersection.hit) {
                res.location = node1_intersection.location;
                res.t = node1_intersection.t;
                res.object_index = node1_intersection.object_index;
                res.side = node1_intersection.side;
                res.hit = true;
                return res;
            }
            else if (node2_intersection.hit) {
                res.location = node2_intersection.location;
                res.t = node2_intersection.t;
                res.object_index = node2_intersection.object_index;
                res.side = node2_intersection.side;
                res.hit = true;
                return res;
            }
            else {
                res.hit = false;
                return res;
            }
        }

        // if 1 is a gameobject and 2 is a gameobject
        if (node.type1 == Type::Entity && node.type2 == Type::Entity) {
            if (t1 < t2) {
                res.location = (ray.direction * t1) + ray.origin;
                res.t = t1;
                res.object_index = node.index1;
                res.side = side1;
                res.hit = true;
                return res;
            }
            else {
                res.location = (ray.direction * t2) + ray.origin;
                res.t = t2;
                res.object_index = node.index2;
                res.side = side2;
                res.hit = true;
                return res;
            }
        }
    }
    else if (is_hit1) {
        switch (node.type1) {
            case Type::BoundingVolume:
                return RaycastTreeNode(ray, bvh_.at(node.index1));
            case Type::Entity:
                res.location = (ray.direction * t1) + ray.origin;
                res.t = t1;
                res.object_index = node.index1;
                res.side = side1;
                res.hit = true;
                return res;
            case Type::Empty:
            default:
                throw std::runtime_error("Invalid enum!");
        }
    }
    else if (is_hit2) {
        switch (node.type2) {
            case Type::BoundingVolume:
                return RaycastTreeNode(ray, bvh_.at(node.index2));
            case Type::Entity:
                res.location = (ray.direction * t2) + ray.origin;
                res.t = t2;
                res.object_index = node.index2;
                res.side = side2;
                res.hit = true;
                return res;
            case Type::Empty:
            default:
                throw std::runtime_error("Invalid enum!");
        }
    }

    throw std::runtime_error("RaycastTreeNode() hit end of function. This should not be possible");
}

} // namespace engine
