#include "util/gltf_loader.h"

#include "log.h"
#include "util/files.h"

#include "libs/mikktspace.h"
#include "libs/weldmesh.h"
#include "libs/tiny_gltf.h"

#include "components/mesh_renderable.h"
#include "components/transform.h"
#include "components/collider.h"

struct Color {
    uint8_t r, g, b, a;
    Color(const std::vector<double>& doubles)
    {
        if (doubles.size() != 4) throw std::runtime_error("Invalid color doubles array");
        r = static_cast<uint8_t>(lround(doubles[0] * 255.0));
        g = static_cast<uint8_t>(lround(doubles[1] * 255.0));
        b = static_cast<uint8_t>(lround(doubles[2] * 255.0));
        a = static_cast<uint8_t>(lround(doubles[3] * 255.0));
    }
    bool operator==(const Color&) const = default;
};

namespace std {
template <>
struct std::hash<Color> {
    std::size_t operator()(const Color& k) const
    {
        return static_cast<size_t>(k.r) << 24 | static_cast<size_t>(k.g) << 16 | static_cast<size_t>(k.b) << 8 | static_cast<size_t>(k.a);
    }
};
} // namespace std

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

    LOG_INFO("Loading gltf file: {}", path);

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

    // check for required extensions
    if (model.extensionsRequired.empty() == false) { // this loader doesn't support any extensions
        for (const auto& ext : model.extensionsRequired) {
            LOG_CRITICAL("Unsupported required extension: {}", ext);
        }
        throw std::runtime_error("One or more required extensions are unsupported. File: " + path);
    }

    // test model loading

    if (model.scenes.size() < 1) {
        throw std::runtime_error("Need at least 1 scene");
    }

    int scene_index = 0;
    if (model.defaultScene != -1) scene_index = model.defaultScene;

    const tg::Scene& s = model.scenes.at(scene_index);

    /* Find which texture indices point to base color maps. */
    /* This must be done for the Texture constructor to know to use srgb or not. */
    std::vector<bool> tex_index_is_base_color(model.textures.size(), false);
    for (const tg::Material& mat : model.materials) {
        int texture_index = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texture_index != -1) {
            assert(texture_index < tex_index_is_base_color.size());
            tex_index_is_base_color[texture_index] = true;
        }
    }

    /* load all textures found in the model */

    std::vector<std::shared_ptr<Texture>> textures{};
    textures.reserve(model.textures.size());

    size_t texture_idx = 0;
    for (const tg::Texture& texture : model.textures) {
        // find the image first
        // use missing texture image by default
        textures.emplace_back(scene.app()->GetResource<Texture>("builtin.white"));

        if (texture.source == -1) {
            LOG_ERROR("A gltf file specifies a texture with no source.");
            continue;
        }

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
            textures.back() = std::make_shared<Texture>(scene.app()->renderer(), image.image.data(), image.width, image.height, samplerInfo,
                                                        tex_index_is_base_color[texture_idx]);
        }
        else {
            throw std::runtime_error("Texture found in gltf file is unsupported! Make sure texture is 8 bpp. File: " + path);
        }

        ++texture_idx;
    }

    /* load all materials found in model */

    // store some 1x1 colour textures as a hack to render solid colours
    std::unordered_map<Color, std::shared_ptr<Texture>> colour_textures;
    // same for metallic roughness:
    std::unordered_map<Color, std::shared_ptr<Texture>> metal_rough_textures;

    std::vector<std::shared_ptr<Material>> materials{};
    materials.reserve(model.materials.size());
    for (const tg::Material& material : model.materials) {
        if (material.alphaMode != "OPAQUE") {
            LOG_WARN("Material {} contains alphaMode {} which isn't supported yet", material.name, material.alphaMode);
        }
        if (material.doubleSided == true) {
            LOG_WARN("Material {} specifies double-sided mesh rendering which isn't supported yet", material.name);
        }
        if (material.emissiveTexture.index != -1 || material.emissiveFactor[0] != 0.0 || material.emissiveFactor[1] != 0.0 ||
            material.emissiveFactor[2] != 0.0) {
            LOG_WARN("Material {} contains an emissive texture or non-zero emissive factor. Emission is currently unsupported.", material.name);
        }
        const auto& baseColorFactor4 = material.pbrMetallicRoughness.baseColorFactor;
        if (baseColorFactor4[0] != 1.0 || baseColorFactor4[1] != 1.0 || baseColorFactor4[2] != 1.0 || baseColorFactor4[3] != 1.0) {
            if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
                LOG_WARN("Material {} contains a base color multiplier which isn't supported yet.", material.name);
            }
        }
        if (material.pbrMetallicRoughness.metallicFactor != 1.0 || material.pbrMetallicRoughness.roughnessFactor != 1.0) {
            if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
                LOG_WARN("Material {} contains a metallic and/or roughness multiplier which isn't supported yet.", material.name);
            }
        }

        materials.emplace_back(std::make_shared<Material>(scene.app()->renderer(), scene.app()->GetResource<Shader>("builtin.fancy")));

        // base color
        materials.back()->SetAlbedoTexture(scene.app()->GetResource<Texture>("builtin.white"));
        if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
            if (material.pbrMetallicRoughness.baseColorTexture.texCoord == 0) {
                materials.back()->SetAlbedoTexture(textures.at(material.pbrMetallicRoughness.baseColorTexture.index));
            }
            else {
                LOG_WARN("Material {} base color texture specifies a UV channel other than zero which is unsupported.", material.name);
            }
        }
        else if (baseColorFactor4[0] != 1.0 || baseColorFactor4[1] != 1.0 || baseColorFactor4[2] != 1.0 || baseColorFactor4[3] != 1.0) {
            LOG_DEBUG("Creating a base-color texture...");
            Color c(baseColorFactor4);
            if (colour_textures.contains(c) == false) {
                const uint8_t pixel[4] = {c.r, c.g, c.b, c.a};
                gfx::SamplerInfo samplerInfo{};
                samplerInfo.minify = gfx::Filter::kNearest;
                samplerInfo.magnify = gfx::Filter::kNearest;
                samplerInfo.mipmap = gfx::Filter::kNearest;
                samplerInfo.anisotropic_filtering = false;
                colour_textures.emplace(std::make_pair(c, std::make_shared<Texture>(scene.app()->renderer(), pixel, 1, 1, samplerInfo, true)));
            }
            materials.back()->SetAlbedoTexture(colour_textures.at(c));
        }

        // occlusion roughness metallic
        materials.back()->SetOcclusionRoughnessMetallicTexture(
            scene.app()->GetResource<Texture>("builtin.white")); // default ao = 1.0, rough = 1.0, metal = 1.0
        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
            if (material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord == 0) {
                LOG_DEBUG("Setting occlusion roughness metallic texture!");
                materials.back()->SetOcclusionRoughnessMetallicTexture(textures.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index));
            }
            else {
                LOG_WARN("Material {} metallic roughness texture specifies a UV channel other than zero which is unsupported.", material.name);
            }
        }
        else {
            LOG_DEBUG("Creating occlusion roughness metallic texture...");
            const std::vector<double> mr_values{1.0f /* no AO */, material.pbrMetallicRoughness.roughnessFactor, material.pbrMetallicRoughness.metallicFactor,
                                                1.0f};
            Color mr(mr_values);
            if (metal_rough_textures.contains(mr) == false) {
                const uint8_t pixel[4] = {mr.r, mr.g, mr.b, mr.a};
                gfx::SamplerInfo samplerInfo{};
                samplerInfo.minify = gfx::Filter::kNearest;
                samplerInfo.magnify = gfx::Filter::kNearest;
                samplerInfo.mipmap = gfx::Filter::kNearest;
                samplerInfo.anisotropic_filtering = false;
                metal_rough_textures.emplace(std::make_pair(mr, std::make_shared<Texture>(scene.app()->renderer(), pixel, 1, 1, samplerInfo, false)));
            }
            materials.back()->SetOcclusionRoughnessMetallicTexture(metal_rough_textures.at(mr));
        }

        // occlusion texture
        // if occlusion texture was same as metallic-roughness texture, this will already have been applied
        if (material.occlusionTexture.index != -1) {
            if (material.occlusionTexture.texCoord == 0) {
                if (material.occlusionTexture.index != material.pbrMetallicRoughness.metallicRoughnessTexture.index) {
                    throw std::runtime_error(std::string("Material ") + material.name +
                                             std::string(" has an ambient occlusion texture different to the metal-rough texture."));
                }
            }
            else {
                LOG_WARN("Material {} occlusion texture specifies a UV channel other than zero which is unsupported.", material.name);
            }
        }

        // normal map
        materials.back()->SetNormalTexture(scene.app()->GetResource<Texture>("builtin.normal"));
        if (material.normalTexture.index != -1) {
            if (material.normalTexture.texCoord == 0) {
                materials.back()->SetNormalTexture(textures.at(material.normalTexture.index));
            }
            else {
                LOG_WARN("Material {} normal texture specifies a UV channel other than zero which is unsupported.", material.name);
            }
        }
    }

    /* load all meshes found in model */

    struct EnginePrimitive {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        AABB aabb;
    };
    std::vector<std::vector<EnginePrimitive>> primitive_arrays{}; // sub-array is all primitives for a given mesh
    primitive_arrays.reserve(model.meshes.size());
    for (const tg::Mesh& mesh : model.meshes) {
        auto& primitive_array = primitive_arrays.emplace_back();
        for (const tg::Primitive& primitive : mesh.primitives) {
            if (primitive.attributes.contains("POSITION")) {
                const tg::Accessor& pos_accessor = model.accessors.at(primitive.attributes.at("POSITION"));

                const size_t original_num_vertices = pos_accessor.count;

                bool generate_tangents = false; // generating tangents creates a new index list and therefore all attribute accessors must be reassigned

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
                    generate_tangents = true;
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

                // TODO: natively support indices of these sizes instead of having to convert to uint32
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

                std::vector<Vertex> vertices;

                if (generate_tangents) {
                    LOG_DEBUG("Generating tangents...");
                    LOG_TRACE("Tangent gen: vtx count before = {} idx count before = {}", original_num_vertices, num_indices);
                    // generate tangents if they're not in the file
                    struct MeshData {
                        Attribute<glm::vec3>* positions;
                        Attribute<glm::vec3>* normals;
                        Attribute<glm::vec2>* uvs;
                        const uint32_t* indices;
                        size_t num_indices;
                        std::vector<Vertex>* new_vertices;
                    };

                    MeshData meshData{};
                    meshData.positions = &positions;
                    meshData.normals = &normals;
                    meshData.uvs = &uv0s;
                    meshData.indices = indices.data();
                    meshData.num_indices = num_indices;
                    meshData.new_vertices = &vertices;
                    vertices.resize(num_indices);

                    SMikkTSpaceInterface mts_interface{};
                    mts_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) -> int {
                        const MeshData* meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                        assert(meshData->num_indices % 3 == 0);
                        return static_cast<int>(meshData->num_indices / 3);
                    };
                    mts_interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const int) -> int { return 3; };
                    mts_interface.m_getPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) -> void {
                        const MeshData* const meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
                        assert(i < meshData->num_indices);
                        const size_t vertex_index = meshData->indices[i];
                        const glm::vec3 pos = meshData->positions->operator[](vertex_index);
                        fvPosOut[0] = pos.x;
                        fvPosOut[1] = pos.y;
                        fvPosOut[2] = pos.z;
                    };
                    mts_interface.m_getNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) -> void {
                        const MeshData* const meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
                        assert(i < meshData->num_indices);
                        const size_t vertex_index = meshData->indices[i];
                        const glm::vec3 norm = meshData->normals->operator[](vertex_index);
                        fvNormOut[0] = norm.x;
                        fvNormOut[1] = norm.y;
                        fvNormOut[2] = norm.z;
                    };
                    mts_interface.m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) -> void {
                        const MeshData* const meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
                        assert(i < meshData->num_indices);
                        const size_t vertex_index = meshData->indices[i];
                        const glm::vec2 uv = meshData->uvs->operator[](vertex_index);
                        fvTexcOut[0] = uv.x;
                        fvTexcOut[1] = uv.y;
                    };
                    mts_interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace,
                                                        const int iVert) -> void {
                        MeshData* const meshData = static_cast<MeshData*>(pContext->m_pUserData);
                        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
                        assert(i < meshData->num_indices);
                        const size_t vertex_index = meshData->indices[i];

                        Vertex& new_v = meshData->new_vertices->operator[](i);

                        new_v.pos = meshData->positions->operator[](vertex_index);
                        new_v.norm = meshData->normals->operator[](vertex_index);
                        new_v.uv = meshData->uvs->operator[](vertex_index);
                        new_v.tangent.x = fvTangent[0];
                        new_v.tangent.y = fvTangent[1];
                        new_v.tangent.z = fvTangent[2];
                        new_v.tangent.w = fSign;
                    };
                    SMikkTSpaceContext mts_context{};
                    mts_context.m_pInterface = &mts_interface;
                    mts_context.m_pUserData = &meshData;

                    bool tan_result = genTangSpaceDefault(&mts_context);
                    if (tan_result == false) throw std::runtime_error("Failed to generate tangents!");

                    // vertices now contains new vertices (possibly with duplicates)
                    // use weldmesh to generate new index list without duplicates

                    std::vector<int> remap_table(num_indices);        // initialised to zeros
                    std::vector<Vertex> vertex_data_out(num_indices); // initialised to zeros

                    const int num_unq_vertices = WeldMesh(remap_table.data(), reinterpret_cast<float*>(vertex_data_out.data()),
                                                          reinterpret_cast<float*>(vertices.data()), static_cast<int>(num_indices), Vertex::FloatsPerVertex());
                    assert(num_unq_vertices >= 0);

                    // get new vertices into the vector
                    vertices.resize(static_cast<size_t>(num_unq_vertices));
                    for (size_t i = 0; i < static_cast<size_t>(num_unq_vertices); ++i) {
                        vertices[i] = vertex_data_out[i];
                    }

                    // get new indices into the vector
                    indices.resize(num_indices); // redundant, for clarity
                    for (size_t i = 0; i < num_indices; ++i) {
                        assert(remap_table[i] >= 0);
                        indices[i] = static_cast<uint32_t>(remap_table[i]);
                    }

                    LOG_TRACE("Tangent gen: vtx count after = {} idx count after = {}", vertices.size(), indices.size());
                }
                else {
                    // combine vertices into one array
                    vertices.clear();
                    vertices.reserve(original_num_vertices);
                    for (size_t i = 0; i < original_num_vertices; ++i) {
                        Vertex v{.pos = positions[i], .norm = normals[i], .tangent = tangents[i], .uv = uv0s[i]};
                        vertices.push_back(v);
                    }
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

                // get AABB
                AABB box{};
                box.min.x = static_cast<float>(pos_accessor.minValues.at(0));
                box.min.y = static_cast<float>(pos_accessor.minValues.at(1));
                box.min.z = static_cast<float>(pos_accessor.minValues.at(2));
                box.max.x = static_cast<float>(pos_accessor.maxValues.at(0));
                box.max.y = static_cast<float>(pos_accessor.maxValues.at(1));
                box.max.z = static_cast<float>(pos_accessor.maxValues.at(2));

                primitive_array.emplace_back(engine_mesh, engine_material, box);
            }
            else {
                // skip primitive's rendering
                continue;
            }
        }
    }

    /* now create the entities and traverse the glTF scene hierarchy */
    const std::filesystem::path filePath(path);
    const std::string name = filePath.stem().string();

    // glTF uses the Y-up convention so the parent object must be rotated to Z-up
    const Entity parent = scene.CreateEntity(name, 0, glm::vec3{}, glm::quat{glm::one_over_root_two<float>(), glm::one_over_root_two<float>(), 0.0f, 0.0f});
    scene.GetTransform(parent)->is_static = isStatic;

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
        t->is_static = isStatic; // if file was imported as static, no imported entities can move!

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
            if (primitives.size() == 1) {
                auto meshren = scene.AddComponent<MeshRenderableComponent>(e);
                meshren->mesh = primitives.front().mesh;
                meshren->material = primitives.front().material;
                auto collider = scene.AddComponent<ColliderComponent>(e);
                collider->aabb = primitives.front().aabb;
            }
            else {
                int i = 0;
                for (const EnginePrimitive& prim : primitives) {
                    auto prim_entity = scene.CreateEntity(std::string("_mesh") + std::to_string(i), e);
                    scene.GetTransform(prim_entity)->is_static = isStatic;
                    auto meshren = scene.AddComponent<MeshRenderableComponent>(prim_entity);
                    meshren->mesh = prim.mesh;
                    meshren->material = prim.material;
                    auto collider = scene.AddComponent<ColliderComponent>(prim_entity);
                    collider->aabb = prim.aabb;
                    ++i;
                }
            }
        }

        for (int i : node.children) {
            generateEntities(e, model.nodes.at(i));
        }
    };

    for (int i : s.nodes) {
        generateEntities(parent, model.nodes.at(i));
    }

    LOG_DEBUG("Loaded glTF file: {}", path);

    return parent;
}

} // namespace engine::util