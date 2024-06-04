#include "gen_tangents.h"

#include <stdexcept>

#include <mikktspace.h>
#include <weldmesh.h>

#include "resource_mesh.h"

namespace engine {

std::vector<uint32_t> genTangents(std::vector<engine::Vertex>& vertices)
{

    using VertList = std::vector<Vertex>;

    SMikkTSpaceInterface mts_interface{};
    mts_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) -> int {
        auto vertices = static_cast<const VertList*>(pContext->m_pUserData);
        return static_cast<int>(vertices->size() / 3);
    };
    mts_interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const int) -> int { return 3; };
    mts_interface.m_getPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) -> void {
        auto vertices = static_cast<const VertList*>(pContext->m_pUserData);
        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
        const glm::vec3 pos = vertices->operator[](i).pos;
        fvPosOut[0] = pos.x;
        fvPosOut[1] = pos.y;
        fvPosOut[2] = pos.z;
    };
    mts_interface.m_getNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) -> void {
        auto vertices = static_cast<const VertList*>(pContext->m_pUserData);
        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
        const glm::vec3 norm = vertices->operator[](i).norm;
        fvNormOut[0] = norm.x;
        fvNormOut[1] = norm.y;
        fvNormOut[2] = norm.z;
    };
    mts_interface.m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) -> void {
        auto vertices = static_cast<const VertList*>(pContext->m_pUserData);
        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
        const glm::vec2 uv = vertices->operator[](i).uv;
        fvTexcOut[0] = uv.x;
        fvTexcOut[1] = uv.y;
    };
    mts_interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace,
                                        const int iVert) -> void {
        auto vertices = static_cast<VertList*>(pContext->m_pUserData);
        const size_t i = static_cast<size_t>(iFace) * 3 + static_cast<size_t>(iVert);
        vertices->operator[](i).tangent.x = fvTangent[0];
        vertices->operator[](i).tangent.y = fvTangent[1];
        vertices->operator[](i).tangent.z = fvTangent[2];
        vertices->operator[](i).tangent.w = fSign;
    };
    SMikkTSpaceContext mts_context{};
    mts_context.m_pInterface = &mts_interface;
    mts_context.m_pUserData = &vertices;

    bool tan_result = genTangSpaceDefault(&mts_context);
    if (tan_result == false) throw std::runtime_error("Failed to generate tangents!");

    // generate new vertex and index list without duplicates:

    std::vector<uint32_t> remap_table(vertices.size());   // initialised to zeros
    std::vector<Vertex> vertex_data_out(vertices.size()); // initialised to zeros

    const int new_vertex_count = WeldMesh(reinterpret_cast<int*>(remap_table.data()), reinterpret_cast<float*>(vertex_data_out.data()),
                                          reinterpret_cast<float*>(vertices.data()), static_cast<int>(vertices.size()), Vertex::floatsPerVertex());
    assert(new_vertex_count >= 0);

    // get new vertices into the vector.
    // This potentially modifies the size of the input 'vector' argument
    vertices.resize(static_cast<size_t>(new_vertex_count));
    for (size_t i = 0; i < static_cast<size_t>(new_vertex_count); ++i) {
        vertices[i] = vertex_data_out[i];
    }

    return remap_table;
}

} // namespace engine