module;
#include <string>
#include <cstdint>
#include <memory>
#include <DirectXMath.h>
module Model;

import ModelLoader;

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;
using DirectX::XMMatrixTranslationFromVector;

namespace gg
{
	uint64_t Mesh::VerticesSizeBytes() const { return static_cast<uint64_t>(vertices.size()) * sizeof(Vertex); }
	uint64_t Mesh::IndicesSizeBytes() const { return static_cast<uint64_t>(indices.size()) * sizeof(uint32_t); }
	uint32_t Mesh::GetVertexCount() const { return static_cast<uint32_t>(vertices.size()); }
	uint32_t Mesh::GetIndexCount() const { return static_cast<uint32_t>(indices.size()); }

	Mesh::Mesh(Mesh&& other) noexcept
		: vertices{ std::move(other.vertices) }
		, indices{ std::move(other.indices) }
	{
	}

	Mesh& Mesh::operator=(Mesh&& other) noexcept
	{
		if (this != &other)
		{
			vertices.clear();
			indices.clear();
			vertices = std::move(other.vertices);
			indices = std::move(other.indices);
		}
		return *this;
	}

	Model::Model(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, std::shared_ptr<Texture> t)
		: shaderProgram{ s }
		, texture{ t }
	{
		LoadMeshes(modelRelativePath, *this);
	}

	Model::Model(Model&& other) noexcept
		: shaderProgram{ std::move(other.shaderProgram) }
		, meshes{ std::move(other.meshes) }
		, texture{ std::move(other.texture) }
	{
	}

	Model& Model::operator=(Model&& other) noexcept
	{
		if (this != &other)
		{
			meshes.clear();

			shaderProgram = std::move(other.shaderProgram);
			meshes = std::move(other.meshes);
			texture = std::move(other.texture);
		}
		return *this;
	}

	void Model::SetPosition(XMVECTOR& position)
	{
		translation = XMMatrixTranslationFromVector(position);
	}

} // namespace gg
