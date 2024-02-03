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

    /* load all textures found in the model */

    std::vector<std::shared_ptr<Texture>> textures{};
    textures.reserve(model.textures.size());

    for (const tg::Texture& texture : model.textures) {
        // find the image first
        // use missing texture image by default
        textures.emplace_back(scene.app()->GetResource<Texture>("builtin.white"));
        
        if (texture.source == -1) continue;

        gfx::SamplerInfo samplerInfo{};
        // default to trilinear filtering even if mipmaps are not specified
        samplerInfo.minify = gfx::Filter::kLinear;
        samplerInfo.magnify = gfx::Filter::kLinear;
        samplerInfo.mipmap = gfx::Filter::kLinear;
        if (texture.sampler != -1) {
            const tg::Sampler& sampler = model.samplers.at(texture.sampler);
            switch (sampler.minFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                samplerInfo.minify = gfx::Filter::kNearest;
                samplerInfo.mipmap = gfx::Filter::kLinear;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                samplerInfo.minify = gfx::Filter::kNearest;
                samplerInfo.mipmap = gfx::Filter::kNearest;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                samplerInfo.minify = gfx::Filter::kLinear;
                samplerInfo.mipmap = gfx::Filter::kLinear;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                samplerInfo.minify = gfx::Filter::kLinear;
                samplerInfo.mipmap = gfx::Filter::kNearest;
                break;
            default:
                break;
            }
            switch (sampler.magFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                samplerInfo.magnify = gfx::Filter::kNearest;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                samplerInfo.magnify = gfx::Filter::kLinear;
                break;
            default:
                break;
            }
        }
        // use aniso if min filter is LINEAR_MIPMAP_LINEAR
        samplerInfo.anisotropic_filtering = (samplerInfo.minify == gfx::Filter::kLinear && samplerInfo.mipmap == gfx::Filter::kLinear);

        const tg::Image& image = model.images.at(texture.source);
        if (image.as_is == false && image.bits == 8 && image.component == 4 && image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            // create texture on GPU
            textures.back() = std::make_shared<Texture>(scene.app()->renderer(), image.image.data(), image.width,
                image.height, samplerInfo, true);
        }
    }

    /* load all materials found in model */

    std::vector<std::shared_ptr<Material>> materials{};
    materials.reserve(model.materials.size());
    for (const tg::Material& material : model.materials) {
        // use default material unless a material is found
        materials.emplace_back(scene.app()->GetResource<Material>("builtin.default"));
    }

    /* load all meshes found in model */

    std::vector<std::shared_ptr<Mesh>> meshes{};
    meshes.reserve(model.meshes.size());
    for (const tg::Mesh& mesh : model.meshes) {
        // placeholder mesh for now
        
    }
    
    const Entity parent =
        scene.CreateEntity("test_node", 0, glm::vec3{}, glm::quat{glm::one_over_root_two<float>(), glm::one_over_root_two<float>(), 0.0f, 0.0f});

    return parent;
}

} // namespace engine::util