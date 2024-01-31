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

static void CreateNodes(engine::Scene& app_scene, const tg::Scene& gl_scene, const tg::Model& gl_model, engine::Entity parent_entity, const tg::Node& node)
{
    static int node_count = 0;
    int node_uuid = node_count++;

    glm::vec3 pos{0.0f, 0.0f, 0.0f};
    glm::quat rot{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    if (node.matrix.size() == 16) {
        const glm::mat4 matrix = MatFromDoubleArray(node.matrix);
        DecomposeTransform(matrix, pos, rot, scale);
    }
    else {
        if (node.translation.size() == 3) {
            pos.x = node.translation[0];
            pos.y = node.translation[1];
            pos.z = node.translation[2];
        }
        if (node.rotation.size() == 4) {
            rot.x = node.rotation[0];
            rot.y = node.rotation[1];
            rot.z = node.rotation[2];
            rot.w = node.rotation[3];
        }
        if (node.scale.size() == 3) {
            scale.x = node.scale[0];
            scale.y = node.scale[1];
            scale.z = node.scale[2];
        }
    }

    engine::Entity entity = app_scene.CreateEntity(std::string("test_node") + std::to_string(node_uuid), parent_entity, pos, rot, scale);

    if (node.mesh >= 0) {
        const tg::Mesh& mesh = gl_model.meshes.at(node.mesh);
        const tg::Primitive& prim = mesh.primitives.front(); // required
        if (prim.mode != TINYGLTF_MODE_TRIANGLES) throw std::runtime_error("glTF loader only supports triangles!");

        const tg::Accessor& indices_accessor = gl_model.accessors.at(prim.indices); // not required. TODO: handle no indices
        size_t indices_int_size = 0;
        if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) indices_int_size = 1;
        if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) indices_int_size = 2;
        if (indices_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) indices_int_size = 4;
        if (indices_int_size == 0) throw std::runtime_error("GLTF parse error!");
        const tg::BufferView& indices_bufferview = gl_model.bufferViews.at(indices_accessor.bufferView); // required for TG
        const size_t indices_byteoffset = indices_accessor.byteOffset + indices_bufferview.byteOffset;
        const tg::Buffer& indices_buffer = gl_model.buffers.at(indices_bufferview.buffer);

        std::unique_ptr<std::vector<uint32_t>> indices = nullptr;
        if (indices_int_size == 4) {
            const uint32_t* const indices_data = reinterpret_cast<const uint32_t*>(indices_buffer.data.data() + indices_byteoffset);
            // in future, let Mesh constructor use spans to avoid unneccesary copy here
            indices = std::make_unique<std::vector<uint32_t>>(indices_data, indices_data + indices_accessor.count);
        }
        else if (indices_int_size == 2) {
            indices = std::make_unique<std::vector<uint32_t>>();
            const uint16_t* const indices_data = reinterpret_cast<const uint16_t*>(indices_buffer.data.data() + indices_byteoffset);
            for (size_t i = 0; i < indices_accessor.count; ++i) {
                indices->push_back(indices_data[i]);
            }
        }

        if (indices == nullptr) {
            throw std::runtime_error("TODO: handle and support this");
        }

        const tg::Accessor& pos_accessor = gl_model.accessors.at(prim.attributes.at("POSITION"));
        if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) throw std::runtime_error("Position att. must be float!");
        if (pos_accessor.type != 3) throw std::runtime_error("Position att. dim. must be 3!");
        const tg::BufferView& pos_bufferview = gl_model.bufferViews.at(pos_accessor.bufferView);
        const size_t pos_byteoffset = pos_accessor.byteOffset + pos_bufferview.byteOffset;
        const size_t pos_bytestride = pos_accessor.ByteStride(pos_bufferview);
        const tg::Buffer& pos_buffer = gl_model.buffers.at(pos_bufferview.buffer);

        const tg::Accessor& norm_accessor = gl_model.accessors.at(prim.attributes.at("NORMAL"));
        if (norm_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) throw std::runtime_error("Normal att. must be float!");
        if (norm_accessor.type != 3) throw std::runtime_error("Normal att. dim. must be 3!");
        const tg::BufferView& norm_bufferview = gl_model.bufferViews.at(norm_accessor.bufferView);
        const size_t norm_byteoffset = norm_accessor.byteOffset + norm_bufferview.byteOffset;
        const size_t norm_bytestride = norm_accessor.ByteStride(norm_bufferview);
        const tg::Buffer& norm_buffer = gl_model.buffers.at(norm_bufferview.buffer);

        std::vector<engine::Vertex> vertices(pos_accessor.count);

        if (prim.attributes.contains("TEXCOORD_0")) {
            const tg::Accessor& uv_accessor = gl_model.accessors.at(prim.attributes.at("TEXCOORD_0"));
            if (uv_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) throw std::runtime_error("UV att. must be float!");
            if (uv_accessor.type != 2) throw std::runtime_error("UV att. dim. must be 2!");
            const tg::BufferView& uv_bufferview = gl_model.bufferViews.at(uv_accessor.bufferView);
            const size_t uv_byteoffset = uv_accessor.byteOffset + uv_bufferview.byteOffset;
            const size_t uv_bytestride = uv_accessor.ByteStride(uv_bufferview);
            const tg::Buffer& uv_buffer = gl_model.buffers.at(uv_bufferview.buffer);
            
            for (size_t i = 0; i < vertices.size(); ++i) {
                vertices[i].uv = *reinterpret_cast<const glm::vec2*>(&uv_buffer.data[uv_byteoffset + uv_bytestride * i]);
            }
        }

        bool has_tangents = false;
        if (prim.attributes.contains("TANGENT")) {
            has_tangents = true;
        }

        // copy everything except tangents and uvs
        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].pos = *reinterpret_cast<const glm::vec3*>(&pos_buffer.data[pos_byteoffset + pos_bytestride * i]);
            vertices[i].norm = *reinterpret_cast<const glm::vec3*>(&norm_buffer.data[norm_byteoffset + norm_bytestride * i]);
        }

        if (has_tangents) {
            const tg::Accessor& tangent_accessor = gl_model.accessors.at(prim.attributes.at("TANGENT"));
            if (tangent_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) throw std::runtime_error("Tangent att. must be float!");
            if (tangent_accessor.type != 4) throw std::runtime_error("Tangent att. dim. must be 4!");
            const tg::BufferView& tangent_bufferview = gl_model.bufferViews.at(tangent_accessor.bufferView);
            const size_t tangent_byteoffset = tangent_accessor.byteOffset + tangent_bufferview.byteOffset;
            const size_t tangent_bytestride = tangent_accessor.ByteStride(tangent_bufferview);
            const tg::Buffer& tangent_buffer = gl_model.buffers.at(tangent_bufferview.buffer);
            for (size_t i = 0; i < vertices.size(); ++i) {
                vertices[i].tangent = *reinterpret_cast<const glm::vec4*>(&tangent_buffer.data[tangent_byteoffset + tangent_bytestride * i]);
            }
        }
        else {
          // generate tangents if they're not in the file
            struct MeshData {
                engine::Vertex* vertices;
                const uint32_t* indices;
                size_t count;
                std::vector<uint32_t> new_indices;
            };

            MeshData meshData{};
            meshData.vertices = vertices.data();
            meshData.indices = indices->data();
            meshData.count = indices->size();
            meshData.new_indices.reserve(meshData.count);

            SMikkTSpaceInterface mts_interface{};
            mts_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) -> int {
                const MeshData* meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                assert(meshData->count % 3 == 0);
                return meshData->count / 3;
            };
            mts_interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const int) -> int { return 3; };
            mts_interface.m_getPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) -> void {
                const MeshData* const meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                const glm::vec3 pos = meshData->vertices[meshData->indices[iFace * 3 + iVert]].pos;
                fvPosOut[0] = pos.x;
                fvPosOut[1] = pos.y;
                fvPosOut[2] = pos.z;
            };
            mts_interface.m_getNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) -> void {
                const MeshData* const meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                const glm::vec3 norm = meshData->vertices[meshData->indices[iFace * 3 + iVert]].norm;
                fvNormOut[0] = norm.x;
                fvNormOut[1] = norm.y;
                fvNormOut[2] = norm.z;
            };
            mts_interface.m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) -> void {
                const MeshData* const meshData = static_cast<const MeshData*>(pContext->m_pUserData);
                const glm::vec2 uv = meshData->vertices[meshData->indices[iFace * 3 + iVert]].uv;
                fvTexcOut[0] = uv.x;
                fvTexcOut[1] = uv.y;
            };
            mts_interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace,
                                                const int iVert) -> void {
                MeshData* const meshData = static_cast<MeshData*>(pContext->m_pUserData);
                glm::vec4& tangent = meshData->vertices[meshData->indices[iFace * 3 + iVert]].tangent;
                tangent.x = fvTangent[0];
                tangent.y = fvTangent[1];
                tangent.z = fvTangent[2];
                tangent.w = fSign;
            };
            SMikkTSpaceContext mts_context{};
            mts_context.m_pInterface = &mts_interface;
            mts_context.m_pUserData = &meshData;

            bool tan_result = genTangSpaceDefault(&mts_context);
            if (tan_result == false) throw std::runtime_error("Failed to generate tangents!");
        }

        auto mesh_comp = app_scene.AddComponent<engine::MeshRenderableComponent>(entity);
        mesh_comp->mesh = std::make_unique<engine::Mesh>(app_scene.app()->renderer()->GetDevice(), vertices, *indices);

        // now get material
        mesh_comp->material = std::make_unique<engine::Material>(app_scene.app()->renderer(), app_scene.app()->GetResource<Shader>("builtin.fancy"));

        mesh_comp->material->SetAlbedoTexture(app_scene.app()->GetResource<Texture>("builtin.white"));
        mesh_comp->material->SetNormalTexture(app_scene.app()->GetResource<Texture>("builtin.normal"));

        if (prim.material >= 0) {
            const tg::Material& mat = gl_model.materials.at(prim.material);
            if (mat.alphaMode != "OPAQUE") {
                LOG_WARN("Non-opaque alpha modes are not supported yet");
            }
            if (mat.doubleSided == true) {
                LOG_WARN("Double-sided materials are not supported yet");
            }
            if (mat.normalTexture.index != -1) {
                if (mat.normalTexture.texCoord == 0) {
                    if (mat.normalTexture.scale == 1.0) {
                        const tg::Texture& norm_texture = gl_model.textures.at(mat.normalTexture.index);
                        if (norm_texture.source != -1) {
                            const tg::Image& norm_image = gl_model.images.at(norm_texture.source);
                            if (norm_image.as_is == false && norm_image.bits == 8 && norm_image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                                // create texture on GPU
                                mesh_comp->material->SetNormalTexture(std::make_unique<Texture>(app_scene.app()->renderer(), norm_image.image.data(),
                                                                                                norm_image.width, norm_image.height,
                                                                                                Texture::Filtering::kAnisotropic, false));
                            }
                        }
                    }
                    else {
                        LOG_WARN("Normal texture has scaling which is unsupported. Ignoring normal map.");
                    }
                }
                else {
                    LOG_WARN("Normal texture doesn't specify UV0. Ignoring normal map.");
                }
            }
            if (mat.pbrMetallicRoughness.baseColorTexture.index != -1) {
                if (mat.pbrMetallicRoughness.baseColorTexture.texCoord == 0) {
                    const tg::Texture& texture = gl_model.textures.at(mat.pbrMetallicRoughness.baseColorTexture.index);
                    if (texture.source != -1) {
                        const tg::Image& image = gl_model.images.at(texture.source);
                        if (image.as_is == false && image.bits == 8 && image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            // create texture on GPU
                            mesh_comp->material->SetAlbedoTexture(std::make_unique<Texture>(app_scene.app()->renderer(), image.image.data(), image.width,
                                                                                            image.height, Texture::Filtering::kAnisotropic, true));
                        }
                    }
                }
                else {
                    LOG_WARN("Color texture doesn't specify UV0. Ignoring.");
                }
            }
        }
    }

    for (const int node : node.children) {
        CreateNodes(app_scene, gl_scene, gl_model, entity, gl_model.nodes.at(node));
    }
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