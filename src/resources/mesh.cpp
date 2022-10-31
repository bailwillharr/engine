#include "resources/mesh.hpp"

#include "gfx_device.hpp"

#include "log.hpp"

namespace engine::resources {

struct MeshFileHeader {
	uint32_t vertex_count;
	uint32_t index_count;
	int32_t material;
};

static void loadMeshFromFile(const std::filesystem::path& path, std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
{

	// TODO
	// Replace this aberation with something that's readable and doesn't use FILE*

	struct MeshFileHeader header{};

	FILE* fp = fopen(path.string().c_str(), "rb");
	if (fp == NULL) {
		throw std::runtime_error("Unable to open mesh file " + path.string());
	}

	fread(&header, sizeof(struct MeshFileHeader), 1, fp);

	indices->resize(header.index_count);
	vertices->resize(header.vertex_count);

	fread(indices->data(), sizeof(uint32_t) * header.index_count, 1, fp);
	fread(vertices->data(), sizeof(float) * 8 * header.vertex_count, 1, fp);
	fclose(fp);

}

void Mesh::initMesh()
{
	vb = gfxdev->createBuffer(gfx::BufferType::VERTEX, m_vertices.size() * sizeof(Vertex), m_vertices.data());
	ib = gfxdev->createBuffer(gfx::BufferType::INDEX, m_indices.size() * sizeof(uint32_t), m_indices.data());

	TRACE("VB PTR in mesh: {}", (void*)vb);

	TRACE("Vertices:");

	for (const auto& v : m_vertices) {
		TRACE("pos: {}, {}, {}", v.pos.x, v.pos.y, v.pos.z);
	}

	TRACE("Indices:");

	for (const uint32_t i : m_indices) {
		TRACE("\t{}", i);
	}
}

Mesh::Mesh(const std::vector<Vertex>& vertices) : Resource("", "mesh")
{
	// constructor for custom meshes without an index array
	m_vertices = vertices; // COPY over vertices
	for (uint32_t i = 0; i < m_vertices.size(); i++) {
		m_indices.push_back(i);
	}
	initMesh();
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) : Resource("", "mesh")
{
	m_vertices = vertices; // COPY over vertices
	m_indices = indices; // COPY over indices;
	initMesh();
}

// To be used with the resource manager
Mesh::Mesh(const std::filesystem::path& resPath) : Resource(resPath, "mesh")
{
	loadMeshFromFile(resPath, &m_vertices, &m_indices);
	initMesh();
}

Mesh::~Mesh()
{
	gfxdev->destroyBuffer(ib);
	gfxdev->destroyBuffer(vb);
}

}
