#include "resources/mesh.hpp"

namespace resources {

struct MeshFileHeader {
	unsigned int vertex_count;
	unsigned int index_count;
	int material;
};

static void loadMeshFromFile(const std::filesystem::path& path, std::vector<Vertex>* vertices, std::vector<unsigned int>* indices)
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

	fread(&(*indices)[0], sizeof(unsigned int) * header.index_count, 1, fp);
	fread(&((*vertices)[0].pos[0]), sizeof(float) * 8 * header.vertex_count, 1, fp);

	fclose(fp);

}

static void loadObjFromFile(const std::filesystem::path& path, std::vector<Vertex>* vertices, std::vector<unsigned int>* indices)
{

}

// -1 means invalidated
int Mesh::s_active_vao = -1;

void Mesh::bindVAO() const
{
	if (s_active_vao != m_vao) {
		glBindVertexArray(m_vao);
		s_active_vao = m_vao;
	}
}

void Mesh::initMesh()
{
	glGenVertexArrays(1, &m_vao);
	bindVAO();
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, m_vertices.size()*sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), &(m_indices[0]), GL_STATIC_DRAW);

	// position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)offsetof(Vertex, pos));
	// normal
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)offsetof(Vertex, norm));
	// uv
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)offsetof(Vertex, uv));

}

void Mesh::drawMesh(const Shader& shader)
{
	bindVAO();
	shader.makeActive();
#ifndef SDLTEST_NOGFX
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, 0);
#endif
}

Mesh::Mesh(const std::vector<Vertex>& vertices) : Resource("", "mesh")
{
	// constructor for custom meshes without an index array
	m_vertices = vertices; // COPY over vertices
	for (int i = 0; i < m_vertices.size(); i++) {
		m_indices.push_back(i);
	}
	initMesh();
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) : Resource("", "mesh")
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
	glDeleteVertexArrays(1, &m_vao);
	glDeleteBuffers(1, &m_vbo);
	glDeleteBuffers(1, &m_ebo);
	if (s_active_vao == m_vao) {
		s_active_vao = -1;
	}
}

}
