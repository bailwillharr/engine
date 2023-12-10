#include "util/gltf_loader.h"

#include "log.h"
#include "util/files.h"

#include "libs/mikktspace.h"
#include "libs/tiny_gltf.h"

#include "components/mesh_renderable.h"
#include <components/transform.h>

namespace tg = tinygltf;

namespace engine::util {

static void DecomposeTransform(glm::mat4 transform, glm::vec3& pos, glm::quat& rot, glm::vec3& scale)
{
    // get position
    pos.x = transform[3][0];
    pos.y = transform[3][1];
    pos.z = transform[3][2];

    // remove position from matrix
    transform[3][0] = 0.0f;
    transform[3][1] = 0.0f;
    transform[3][2] = 0.0f;

    // get scale
    scale.x = sqrtf(transform[0][0] * transform[0][0] + transform[0][1] * transform[0][1] + transform[0][2] * transform[0][2]);
    scale.y = sqrtf(transform[1][0] * transform[1][0] + transform[1][1] * transform[1][1] + transform[1][2] * transform[1][2]);
    scale.z = sqrtf(transform[2][0] * transform[2][0] + transform[2][1] * transform[2][1] + transform[2][2] * transform[2][2]);

    // remove scaling from matrix
    for (int row = 0; row < 3; row++) {
        transform[0][row] /= scale.x;
        transform[1][row] /= scale.y;
        transform[2][row] /= scale.z;
    }

    // get rotation
    rot = glm::quat_cast(transform);
}

static glm::mat4 MatFromDoubleArray(const std::vector<double>& arr)
{
    glm::mat4 mat{};
    for (int i = 0; i < 4; ++i) {
        mat[i][0] = static_cast<float>(arr[i * 4 + 0]);
        mat[i][1] = static_cast<float>(arr[i * 4 + 1]);
        mat[i][2] = static_cast<float>(arr[i * 4 + 2]);
        mat[i][3] = static_cast<float>(arr[i * 4 + 3]);
    }
    return mat;
}

static void CreateNodes(engine::Scene& app_scene, const tg::Scene& gl_scene, const tg::Model& gl_model, engine::Entity parent_entity,
                        const tg::Node& node)
{
    static int node_uuid = 0;

    const glm::mat4 matrix = MatFromDoubleArray(node.matrix);
    glm::vec3 pos;
    glm::quat rot;
    glm::vec3 scale;
    DecomposeTransform(matrix, pos, rot, scale);

    engine::Entity entity = app_scene.CreateEntity(std::string("test_node") + std::to_string(node_uuid), parent_entity, pos, rot, scale);

    if (node.mesh >= 0) {
        const tg::Mesh& mesh = gl_model.meshes.at(node.mesh);
        const tg::Primitive& prim = mesh.primitives.front();
        const tg::Accessor& indices_accessor = gl_model.accessors.at(prim.indices);
        const tg::BufferView& indices_bufferview = gl_model.bufferViews.at(indices_accessor.bufferView);
    }

    for (const int node : node.children) {
        CreateNodes(app_scene, gl_scene, gl_model, entity, gl_model.nodes.at(node));
    }

    ++node_uuid;
}

engine::Entity LoadGLTF(Scene& scene, const std::string& path, bool isStatic)
{

    tg::TinyGLTF loader;
    tg::Model model;
    std::string err, warn;

    loader.SetParseStrictness(tg::ParseStrictness::Strict);

    const bool success = loader.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!warn.empty()) {
        LOG_WARN("glTF Loader: {}", warn);
    }

    if (!err.empty()) {
        LOG_ERROR("glTF Loader: {}", err);
    }

    if (!success) {
        throw std::runtime_error("Failed to load glTF file!");
    }

    LOG_INFO("Loaded glTF model, contains {} scenes", model.scenes.size());

    // test model loading

    if (model.scenes.size() < 1) {
        throw std::runtime_error("Need at least 1 scene");
    }

    int scene_index = 0;
    if (model.defaultScene != -1) scene_index = model.defaultScene;

    const tg::Scene& s = model.scenes.at(scene_index);

    const Entity parent =
        scene.CreateEntity("test_node", 0, glm::vec3{}, glm::quat{glm::one_over_root_two<float>(), glm::one_over_root_two<float>(), 0.0f, 0.0f});

    for (int node : s.nodes) {
        CreateNodes(scene, s, model, parent, model.nodes.at(node));
    }

    return parent;
}

} // namespace engine::util