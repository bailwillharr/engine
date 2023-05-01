#include "resources/mesh.hpp"

#include "log.hpp"
#include "gfx_device.hpp"

namespace engine::resources {

	Mesh::Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices)
		: gfx_(gfx)
	{
		std::vector<uint32_t> indices(vertices.size());
		for (uint32_t i = 0; i < indices.size(); i++) {
			indices[i] = i;
		}
		InitMesh(vertices, indices);
	}

	Mesh::~Mesh()
	{
		gfx_->DestroyBuffer(ib_);
		gfx_->DestroyBuffer(vb_);
	}

	const gfx::Buffer* Mesh::GetVB()
	{
		return vb_;
	}

	const gfx::Buffer* Mesh::GetIB()
	{
		return ib_;
	}

	uint32_t Mesh::GetCount()
	{
		return count_;
	}

	void Mesh::InitMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
		vb_ = gfx_->CreateBuffer(gfx::BufferType::kVertex, vertices.size() * sizeof(Vertex), vertices.data());
		ib_ = gfx_->CreateBuffer(gfx::BufferType::kIndex, indices.size() * sizeof(uint32_t), indices.data());
		count_ = (uint32_t)indices.size();
		LOG_INFO("Loaded mesh, vertices: {}, indices: {}", vertices.size(), indices.size());
	}

}
