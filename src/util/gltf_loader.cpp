#include "util/gltf_loader.h"

#include "log.h"
#include "util/files.h"

#include "libs/mikktspace.h"
#include "libs/tiny_gltf.h"

#include "components/mesh_renderable.h"
#include <components/transform.h>

namespace tg = tinygltf;

namespace engine::util {

template <typename T>
struct Attribute {
    const uint8_t* buffer;
    size_t offset;
    size_t stride;
    const T& operator[](size_t i) { return *reinterpret_cast<const T*>(&buffer[offset + stride * i]); }
};

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
        mat[i][0] = static_cast<float>(arr[static_cast<size_t>(i) * 4 + 0]);
        mat[i][1] = static_cast<float>(arr[static_cast<size_t>(i) * 4 + 1]);
        mat[i][2] = static_cast<float>(arr[static_cast<size_t>(i) * 4 + 2]);
        mat[i][3] = static_cast<float>(arr[static_cast<size_t>(i) * 4 + 3]);
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
            // TODO: somehow detect if the textue should be srgb or not
            textures.back() = std::make_shared<Texture>(scene.app()->renderer(), image.image.data(), image.width, image.height, samplerInfo, true);
        }
    }

    /* load all materials found in model */

    std::vector<std::shared_ptr<Material>> materials{};
    materials.reserve(model.materials.size());
    for (const tg::Material& material : model.materials) {
        if (material.alphaMode != "OPAQUE") {
            LOG_WARN("Material {} contains alphaMode {} which isn't supported yet", material.name, material.alphaMode);
            LOG_WARN("Material will be opaque");
        }
        if (material.doubleSided == true) {
            LOG_WARN("Material {} specifies double-sided mesh rendering which isn't supported yet", material.name);
            LOG_WARN("Material will be single-sided.");
        }
        if (material.emissiveTexture.index != -1 || material.emissiveFactor[0] != 0.0 || material.emissiveFactor[1] != 0.0 ||
            material.emissiveFactor[2] != 0.0) {
            LOG_WARN("Material {} contains an emissive texture or non-zero emissive factor. Emission is currently unsupported.", material.name);
            LOG_WARN("Material will be created without emission.");
        }
        if (material.occlusionTexture.index != -1) {
            LOG_WARN("Material {} contains an ambient occlusion texture which isn't supported yet.", material.name);
            LOG_WARN("Material will be created without an occlusion map.");
        }
        const auto& baseColorFactor4 = material.pbrMetallicRoughness.baseColorFactor;
        if (baseColorFactor4[0] != 1.0 || baseColorFactor4[1] != 1.0 || baseColorFactor4[2] != 1.0 || baseColorFactor4[3] != 1.0) {
            LOG_WARN("Material {} contains a base color value which isn't supported yet.", material.name);
            if (material.pbrMetallicRoughness.baseColorTexture.index == -1) {
                LOG_WARN("Material will be created with a white base color texture.");
            }
            else {
                LOG_WARN("The material's base color texture will be used as-is.");
            }
        }
        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
            LOG_WARN("Material {} contains a metallic-roughness texture which isn't supported yet.", material.name);
            LOG_WARN("This texture will be ignored.");
        }
        if (material.pbrMetallicRoughness.metallicFactor != 1.0) {
            LOG_WARN("Material {} contains a metallic factor != 1.0 which isn't supported yet.", material.name);
            LOG_WARN("Material will be created as fully metallic");
        }
        if (material.pbrMetallicRoughness.roughnessFactor != 1.0) {
            LOG_WARN("Material {} contains a roughness factor != 1.0 which isn't supported yet.", material.name);
            LOG_WARN("Material will be created as fully rough");
        }

        materials.emplace_back(std::make_shared<Material>(scene.app()->renderer(), scene.app()->GetResource<Shader>("builtin.fancy")));
        materials.back()->SetAlbedoTexture(scene.app()->GetResource<Texture>("builtin.white"));
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            if (material.pbrMetallicRoughness.baseColorTexture.texCoord == 0) {
                materials.back()->SetAlbedoTexture(textures.at(material.pbrMetallicRoughness.baseColorTexture.index));
            }
            else {
                LOG_WARN("Material {} base color texture specifies a UV channel other than zero which is unsupported.");
                LOG_WARN("Material will be created with a white base color");
            }
        }

        materials.back()->SetNormalTexture(scene.app()->GetResource<Texture>("builtin.normal"));
        if (material.normalTexture.index != -1) {
            if (material.normalTexture.texCoord == 0) {
                materials.back()->SetNormalTexture(textures.at(material.normalTexture.index));
            }
            else {
                LOG_WARN("Material {} normal texture specifies a UV channel other than zero which is unsupported.");
                LOG_WARN("Material will be created with no normal map");
            }
        }
    }

    /* load all meshes found in model */

    struct EnginePrimitive {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
    };
    std::vector<std::vector<EnginePrimitive>> primitive_arrays{}; // sub-array is all primitives for a given mesh
    primitive_arrays.reserve(model.meshes.size());
    for (const tg::Mesh& mesh : model.meshes) {
        auto& primitive_array = primitive_arrays.emplace_back();
        for (const tg::Primitive& primitive : mesh.primitives) {
            if (primitive.attributes.contains("POSITION")) {
                const tg::Accessor& pos_accessor = model.accessors.at(primitive.attributes.at("POSITION"));

                const size_t num_vertices = pos_accessor.count;

                // these checks are probably unneccesary assuming a valid glTF file
                // if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) throw std::runtime_error("Position att. must be float!");
                // if (pos_accessor.type != 3) throw std::runtime_error("Position att. dim. must be 3!");

                const tg::BufferView& pos_bufferview = model.bufferViews.at(pos_accessor.bufferView);
                const tg::Buffer& pos_buffer = model.buffers.at(pos_bufferview.buffer);

                Attribute<glm::vec3> positions{.buffer = pos_buffer.data.data(),
                                               .offset = pos_accessor.byteOffset + pos_bufferview.byteOffset,
                                               .stride = static_cast<size_t>(pos_accessor.ByteStride(pos_bufferview))};

                Attribute<glm::vec3> normals{};
                if (primitive.attributes.contains("NORMAL")) {
                    const tg::Accessor& norm_accessor = model.accessors.at(primitive.attributes.at("NORMAL"));
                    const tg::BufferView& norm_bufferview = model.bufferViews.at(norm_accessor.bufferView);
                    const tg::Buffer& norm_buffer = model.buffers.at(norm_bufferview.buffer);
                    normals.buffer = norm_buffer.data.data();
                    normals.offset = norm_accessor.byteOffset + norm_bufferview.byteOffset;
                    normals.stride = static_cast<size_t>(norm_accessor.ByteStride(norm_bufferview));
                }
                else {
                    // TODO: generate flat normals
                    throw std::runtime_error(std::string("No normals found in primitive from ") + mesh.name);
                }

                Attribute<glm::vec4> tangents{};
                if (primitive.attributes.contains("TANGENT")) {
                    const tg::Accessor& tang_accessor = model.accessors.at(primitive.attributes.at("TANGENT"));
                    const tg::BufferView& tang_bufferview = model.bufferViews.at(tang_accessor.bufferView);
                    const tg::Buffer& tang_buffer = model.buffers.at(tang_bufferview.buffer);
                    tangents.buffer = tang_buffer.data.data();
                    tangents.offset = tang_accessor.byteOffset + tang_bufferview.byteOffset;
                    tangents.stride = static_cast<size_t>(tang_accessor.ByteStride(tang_bufferview));
                }
                else {
                    // TODO: use MikkTSpace to generate tangents
                    throw std::runtime_error(std::string("No tangents found in primitive from ") + mesh.name);
                }

                // UV0
                Attribute<glm::vec2> uv0s{};
                if (primitive.attributes.contains("TEXCOORD_0")) {
                    const tg::Accessor& uv0_accessor = model.accessors.at(primitive.attributes.at("TEXCOORD_0"));
                    const tg::BufferView& uv0_bufferview = model.bufferViews.at(uv0_accessor.bufferView);
                    const tg::Buffer& uv0_buffer = model.buffers.at(uv0_bufferview.buffer);
                    uv0s.buffer = uv0_buffer.data.data();
                    uv0s.offset = uv0_accessor.byteOffset + uv0_bufferview.byteOffset;
                    uv0s.stride = static_cast<size_t>(uv0_accessor.ByteStride(uv0_bufferview));
                }
                else {
                    // TODO: Possibly create a shader variant that doesn't need UVs?
                    throw std::runtime_error(std::string("No TEXCOORD_0 found in primitive from ") + mesh.name);
                }

                // Indices
                const tg::Accessor& indices_accessor = model.accessors.at(primitive.indices);
                const tg::BufferView& indices_bufferview = model.bufferViews.at(indices_accessor.bufferView);
                const tg::Buffer& indices_buffer = model.buffers.at(indices_bufferview.buffer);
                const uint8_t* const indices_data_start = indices_buffer.data.data() + indices_accessor.byteOffset + indices_bufferview.byteOffset;

                const size_t num_indices = indices_accessor.count;
                std::vector<uint32_t> indices;
                indices.reserve(num_indices);

                if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    for (size_t i = 0; i < num_indices; ++i) {
                        indices.push_back(*reinterpret_cast<const uint8_t*>(&indices_data_start[i * 1]));
                    }
                }
                else if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    for (size_t i = 0; i < num_indices; ++i) {
                        indices.push_back(*reinterpret_cast<const uint16_t*>(&indices_data_start[i * 2]));
                    }
                }
                else if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    for (size_t i = 0; i < num_indices; ++i) {
                        indices.push_back(*reinterpret_cast<const uint32_t*>(&indices_data_start[i * 4]));
                    }
                }
                else {
                    throw std::runtime_error(std::string("Invalid index buffer in primtive from: ") + mesh.name);
                }

                // combine vertices into one array
                std::vector<Vertex> vertices;
                vertices.reserve(num_vertices);
                for (size_t i = 0; i < num_vertices; ++i) {
                    Vertex v{.pos = positions[i], .norm = normals[i], .tangent = tangents[i], .uv = uv0s[i]};
                    vertices.push_back(v);
                }

                // generate mesh on GPU
                std::shared_ptr<Mesh> engine_mesh = std::make_shared<Mesh>(scene.app()->renderer()->GetDevice(), vertices, indices);

                // get material
                std::shared_ptr<Material> engine_material = nullptr;
                if (primitive.material != -1) {
                    engine_material = materials.at(primitive.material);
                }
                else {
                    engine_material = scene.app()->GetResource<Material>("builtin.default");
                }

                primitive_array.emplace_back(engine_mesh, engine_material);
            }
            else {
                // skip primitive's rendering
                continue;
            }
        }
    }

    // glTF uses the Y-up convention so objects must be rotated to Z-up
    const Entity parent =
        scene.CreateEntity("test_node", 0, glm::vec3{}, glm::quat{glm::one_over_root_two<float>(), glm::one_over_root_two<float>(), 0.0f, 0.0f});

    std::vector<Entity> entities(model.nodes.size(), 0);
    std::function<void(Entity, const tg::Node&)> generateEntities = [&](Entity parent_entity, const tg::Node& node) -> void {
        const Entity e = scene.CreateEntity(node.name.empty() ? "anode" : node.name, parent_entity);

        // transform
        auto t = scene.GetComponent<TransformComponent>(e);
        t->position.x = 0.0f;
        t->position.y = 0.0f;
        t->position.z = 0.0f;
        t->rotation.x = 0.0f;
        t->rotation.y = 0.0f;
        t->rotation.z = 0.0f;
        t->rotation.w = 1.0f;
        t->scale.x = 1.0f;
        t->scale.y = 1.0f;
        t->scale.z = 1.0f;

        if (node.matrix.size() == 16) {
            const glm::mat4 matrix = MatFromDoubleArray(node.matrix);
            DecomposeTransform(matrix, t->position, t->rotation, t->scale);
        }
        else {
            if (node.translation.size() == 3) {
                t->position.x = static_cast<float>(node.translation[0]);
                t->position.y = static_cast<float>(node.translation[1]);
                t->position.z = static_cast<float>(node.translation[2]);
            }
            if (node.rotation.size() == 4) {
                t->rotation.x = static_cast<float>(node.rotation[0]);
                t->rotation.y = static_cast<float>(node.rotation[1]);
                t->rotation.z = static_cast<float>(node.rotation[2]);
                t->rotation.w = static_cast<float>(node.rotation[3]);
            }
            if (node.scale.size() == 3) {
                t->scale.x = static_cast<float>(node.scale[0]);
                t->scale.y = static_cast<float>(node.scale[1]);
                t->scale.z = static_cast<float>(node.scale[2]);
            }
        }

        // ignoring cameras
        // ignoring skin
        // ignoring weights

        if (node.mesh != -1) {
            const auto& primitives = primitive_arrays.at(node.mesh);
            int i = 0;
            for (const EnginePrimitive& prim : primitives) {
                auto prim_entity = scene.CreateEntity(std::string("_mesh") + std::to_string(i), e);
                auto meshren = scene.AddComponent<MeshRenderableComponent>(prim_entity);
                meshren->mesh = prim.mesh;
                meshren->material = prim.material;
                ++i;
            }
        }

        for (int i : node.children) {
            generateEntities(e, model.nodes.at(i));
        }
    };

    for (int i : s.nodes) {
        generateEntities(parent, model.nodes.at(i));
    }

    LOG_INFO("Loaded glTF model: {}", path);

    return parent;
}

} // namespace engine::util