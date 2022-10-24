#include "resources/mesh.hpp"

#include "gfx_device.hpp"

namespace engine::resources {

struct MeshFileHeader {
	unsigned int vertex_count;
	unsigned int index_count;
	int material;
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

	fread(indices->data(), sizeof(unsigned int) * header.index_count, 1, fp);
	fread(vertices->data(), sizeof(float) * 8 * header.vertex_count, 1, fp);
	fclose(fp);

}

void Mesh::initMesh()
{
	vb = gfx->createBuffer(gfx::BufferType::VERTEX, m_vertices.size() * sizeof(Vertex), m_vertices.data());
	ib = gfx->createBuffer(gfx::BufferType::INDEX, m_indices.size() * sizeof(uint32_t), m_indices.data());
}

void Mesh::drawMesh(const gfx::Pipeline* pipeline)
{
	gfx->draw(pipeline, vb, ib, m_indices.size(), nullptr);
}

Mesh::Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices) : Resource("", "mesh"), gfx(gfx)
{
	// constructor for custom meshes without an index array
	m_vertices = vertices; // COPY over vertices
	for (uint32_t i = 0; i < m_vertices.size(); i++) {
		m_indices.push_back(i);
	}
	initMesh();
}

Mesh::Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) : Resource("", "mesh"), gfx(gfx)
{
	m_vertices = vertices; // COPY over vertices
	m_indices = indices; // COPY over indices;
	initMesh();
}

// To be used with the resource manager
Mesh::Mesh(GFXDevice* gfx, const std::filesystem::path& resPath) : Resource(resPath, "mesh"), gfx(gfx)
{
	loadMeshFromFile(resPath, &m_vertices, &m_indices);
	initMesh();
}

Mesh::~Mesh()
{
	gfx->destroyBuffer(ib);
	gfx->destroyBuffer(vb);
}

}
