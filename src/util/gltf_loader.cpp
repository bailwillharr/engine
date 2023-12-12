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
    static int node_uuid = 0;

    const glm::mat4 matrix = MatFromDoubleArray(node.matrix);
    glm::vec3 pos;
    glm::quat rot;
    glm::vec3 scale;
    DecomposeTransform(matrix, pos, rot, scale);

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
        if (indices_int_size != 4) throw std::runtime_error("TODO: handle and support this");
        const uint32_t* const indices_data = reinterpret_cast<const uint32_t*>(indices_buffer.data.data() + indices_byteoffset);
        // in future, let Mesh constructor use spans to avoid unneccesary copy here
        const std::vector<uint32_t> indices(indices_data, indices_data + indices_accessor.count);

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

        const tg::Accessor& uv_accessor = gl_model.accessors.at(prim.attributes.at("TEXCOORD_0"));
        if (uv_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) throw std::runtime_error("UV att. must be float!");
        if (uv_accessor.type != 2) throw std::runtime_error("UV att. dim. must be 2!");
        const tg::BufferView& uv_bufferview = gl_model.bufferViews.at(uv_accessor.bufferView);
        const size_t uv_byteoffset = uv_accessor.byteOffset + uv_bufferview.byteOffset;
        const size_t uv_bytestride = uv_accessor.ByteStride(uv_bufferview);
        const tg::Buffer& uv_buffer = gl_model.buffers.at(uv_bufferview.buffer);

        std::vector<engine::Vertex> vertices(pos_accessor.count);
        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].pos = *reinterpret_cast<const glm::vec3*>(&pos_buffer.data[pos_byteoffset + pos_bytestride * i]);
            vertices[i].norm = *reinterpret_cast<const glm::vec3*>(&norm_buffer.data[norm_byteoffset + norm_bytestride * i]);
            vertices[i].uv = *reinterpret_cast<const glm::vec2*>(&uv_buffer.data[uv_byteoffset + uv_bytestride * i]);
        }

        // create tangents

        struct MeshData {
            engine::Vertex* vertices;
            const uint32_t* indices;
            size_t count;
        };

        MeshData meshData{};
        meshData.vertices = vertices.data();
        meshData.indices = indices.data();
        meshData.count = indices.size();

        SMikkTSpaceInterface mts_interface{};
        mts_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) -> int { return static_cast<const MeshData*>(pContext->m_pUserData)->count / 3; };
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

        auto mesh_comp = app_scene.AddComponent<engine::MeshRenderableComponent>(entity);
        mesh_comp->mesh = std::make_unique<engine::Mesh>(app_scene.app()->renderer()->GetDevice(), vertices, indices);
        mesh_comp->material = std::make_unique<engine::Material>(app_scene.app()->renderer(), app_scene.app()->GetResource<Shader>("builtin.fancy"));
        mesh_comp->material->SetAlbedoTexture(app_scene.app()->GetResource<Texture>("builtin.white"));
        mesh_comp->material->SetNormalTexture(app_scene.app()->GetResource<Texture>("builtin.normal"));
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