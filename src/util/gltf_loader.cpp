#include "util/gltf_loader.h"

#include "log.h"
#include "util/files.h"

#include "libs/tiny_gltf.h"

#include "components/mesh_renderable.h"

namespace tg = tinygltf;

namespace engine::util {

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

    tg::Scene& s = model.scenes.at(scene_index);

    if (s.nodes.size() < 1) {
        throw std::runtime_error("Need at least 1 node in the scene");
    }

    const tg::Node& node = model.nodes.at(s.nodes[0]);

    const Entity e = scene.CreateEntity("test_node", 0);

    //const tg::Mesh& gt_mesh = model.meshes.at(0);

    //std::vector<uint32_t> indices;

    //model.buffers[0].

    //auto mesh = std::make_unique<Mesh>(scene.app()->renderer()->GetDevice(), vertices, indices);

    //auto mr = scene.AddComponent<MeshRenderableComponent>(e);
    


    // if (node.mesh)

    return static_cast<Entity>(0u);
}

} // namespace engine::util