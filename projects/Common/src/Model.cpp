module;
#include <string>
#include <cstdint>
#include <memory>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "glm/ext/matrix_transform.hpp"
module Model;

import ModelLoader;

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

	Model::Model(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, glm::vec3& position)
		: shaderProgram{ s }
		, translation{ glm::translate(glm::identity<glm::mat4x4>(), position) }
	{
		LoadData(modelRelativePath, *this);
	}

	Model::Model(Model&& other) noexcept
		: shaderProgram{ std::move(other.shaderProgram) }
		, meshes{ std::move(other.meshes) }
		, textures{ std::move(other.textures) }
	{
	}

	Model& Model::operator=(Model&& other) noexcept
	{
		if (this != &other)
		{
			meshes.clear();

			shaderProgram = std::move(other.shaderProgram);
			meshes = std::move(other.meshes);
			textures = std::move(other.textures);
		}
		return *this;
	}

	void Model::SetPosition(glm::vec3& position)
	{
		translation = glm::translate(glm::identity<glm::mat4x4>(), position);
	}

} // namespace gg
