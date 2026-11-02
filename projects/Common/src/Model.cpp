module;
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cstdint>
#include <filesystem>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>
module Model;

import ErrorHandling;

using gg::AssetLoadException;
using gg::BreakIfFalse;

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
		: translation{ glm::translate(glm::identity<glm::mat4x4>(), position) }
		, shaderProgram{ s }
		, modelAbsolutePath{ std::filesystem::absolute(modelRelativePath).generic_string() }
	{
	}

	void Model::Initialize()
	{
		// Make sure assimp is compiled with single-precision
		BreakIfFalse(sizeof(ai_real) == sizeof(float));

		Assimp::Importer importer;
		// Assimp is throwing a DeadlyImportError because of a bug in glb2.0 reader. Does not affect anything.
		aiScene const* scene = importer.ReadFile(modelAbsolutePath,
			aiProcess_Triangulate
			| aiProcess_JoinIdenticalVertices
			| aiProcess_SortByPType
			| aiProcess_FlipUVs
		);
		if (!scene)
			throw AssetLoadException(std::format("Failed to read the input model: {}, error: {}", modelAbsolutePath, importer.GetErrorString()));

		BreakIfFalse(scene->mNumMeshes > 0);
		name = scene->mMeshes[0]->mName.C_Str();
		/* Load meshes */
		for (unsigned int i{ 0 }; i < scene->mNumMeshes; ++i)
		{
			aiMesh const* assimpMesh{ scene->mMeshes[i] };
			constexpr uint32_t UVsetNumber{ 0 };
			BreakIfFalse(assimpMesh->HasPositions());
			BreakIfFalse(assimpMesh->HasTextureCoords(UVsetNumber));
			BreakIfFalse(assimpMesh->HasNormals());

			bool const hasTextureCoords = assimpMesh->HasTextureCoords(UVsetNumber);
			Mesh mesh{};
			mesh.vertices.reserve(assimpMesh->mNumVertices);
			mesh.indices.reserve(assimpMesh->mNumFaces * 3);

			for (unsigned int faceIndex{ 0 }; faceIndex < assimpMesh->mNumFaces; ++faceIndex)
			{
				aiFace* face{ &assimpMesh->mFaces[faceIndex] };
				for (unsigned int j{ 0 }; j < face->mNumIndices; ++j)
				{
					auto vertexIndex = face->mIndices[j];
					auto const& pos = assimpMesh->mVertices[vertexIndex];
					auto const& normal = assimpMesh->mNormals[vertexIndex];

					auto const textureCoords = hasTextureCoords
						? assimpMesh->mTextureCoords[UVsetNumber][vertexIndex]
						: aiVector3D{ 0, 0, 0 };

					mesh.vertices.emplace_back(
						static_cast<float>(pos.x),
						static_cast<float>(pos.y),
						static_cast<float>(pos.z),
						1.0f, // w
						static_cast<float>(textureCoords.x),
						static_cast<float>(textureCoords.y),
						static_cast<float>(normal.x),
						static_cast<float>(normal.y),
						static_cast<float>(normal.z),
						0.0f
					);
				}
			}
			meshes.emplace_back(std::move(mesh));
		}
		/* Save texture names */
		for (uint32_t i = 0; i < scene->mNumTextures; ++i)
			textureNames.emplace_back(scene->mTextures[i]->mFilename.C_Str());

#ifdef NDEBUG
		std::string{}.swap(modelAbsolutePath);
#endif
	}

	Model::Model(Model&& other) noexcept
		: translation{ other.translation }
		, shaderProgram{ std::move(other.shaderProgram) }
		, textureNames{ std::move(other.textureNames) }
		, textures{ std::move(other.textures) }
		, meshes{ std::move(other.meshes) }
		, name{ std::move(other.name) }
		, modelAbsolutePath{ std::move(other.modelAbsolutePath) }
	{
	}

	Model& Model::operator=(Model&& other) noexcept
	{
		if (this != &other)
		{
			translation = other.translation;
			shaderProgram = std::move(other.shaderProgram);
			textureNames = std::move(other.textureNames);
			textures = std::move(other.textures);
			meshes = std::move(other.meshes);
			name = std::move(other.name);
			modelAbsolutePath = std::move(other.modelAbsolutePath);
		}
		return *this;
	}

	void Model::SetPosition(glm::vec3& position)
	{
		translation = glm::translate(glm::identity<glm::mat4x4>(), position);
	}

} // namespace gg
