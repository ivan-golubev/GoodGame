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

using gg::BreakIfFalse;

namespace
{
	std::expected<aiScene const*, gg::Error> readScene(std::string const& modelAbsolutePath, Assimp::Importer& importer)
	{
		// Assimp is throwing a DeadlyImportError because of a bug in glb2.0 reader. Does not affect anything.
		aiScene const* scene = importer.ReadFile(modelAbsolutePath,
			aiProcess_Triangulate
			| aiProcess_JoinIdenticalVertices
			| aiProcess_SortByPType
			| aiProcess_FlipUVs
		);
		if (!scene)
			return std::unexpected(gg::Error{
				.code = gg::ErrorCode::ModelImportFailed,
				.message = std::format("Failed to import model '{}': {}", modelAbsolutePath, importer.GetErrorString()),
			});

		if (scene->mNumMeshes == 0)
			return std::unexpected(gg::Error{
				.code = gg::ErrorCode::ModelContainsNoMeshes,
				.message = std::format("No meshes were found in model '{}'", modelAbsolutePath),
			});
		return scene;
	}

	std::expected<gg::Mesh, gg::Error> readMesh(aiMesh const* assimpMesh)
	{
		constexpr uint32_t UVsetNumber{ 0 };
		if (!assimpMesh->HasPositions())
			return std::unexpected(gg::Error{
				.code = gg::ErrorCode::ModelMeshMissingPositions,
				.message = std::format("Mesh '{}' is missing vertex positions", assimpMesh->mName.C_Str()),
			});
		if (!assimpMesh->HasTextureCoords(UVsetNumber))
			return std::unexpected(gg::Error{
				.code = gg::ErrorCode::ModelMeshMissingTextureCoordinates,
				.message = std::format("Mesh '{}' is missing texture coordinates in UV set {}", assimpMesh->mName.C_Str(), UVsetNumber),
			});
		if (!assimpMesh->HasNormals())
			return std::unexpected(gg::Error{
				.code = gg::ErrorCode::ModelMeshMissingNormals,
				.message = std::format("Mesh '{}' is missing vertex normals", assimpMesh->mName.C_Str()),
			});

		bool const hasTextureCoords = assimpMesh->HasTextureCoords(UVsetNumber);
		gg::Mesh mesh{};
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
		return mesh;
	}

	std::expected<void, gg::Error> loadMeshes(aiScene const& scene, std::vector<gg::Mesh>& meshes)
	{
		for (unsigned int i{ 0 }; i < scene.mNumMeshes; ++i)
		{
			auto mesh{ readMesh(scene.mMeshes[i]) };
			if (!mesh)
				return std::unexpected(mesh.error());
			meshes.emplace_back(std::move(*mesh));
		}
		return {};
	}

	void loadTextureNames(aiScene const& scene, std::vector<std::string>& textureNames)
	{
		for (uint32_t i = 0; i < scene.mNumTextures; ++i)
			textureNames.emplace_back(scene.mTextures[i]->mFilename.C_Str());
	}
}

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

	std::expected<void, Error> Model::Initialize()
	{
		// Make sure assimp is compiled with single-precision
		BreakIfFalse(sizeof(ai_real) == sizeof(float));

		Assimp::Importer importer;
		auto sceneResult{ readScene(modelAbsolutePath, importer) };
		if (!sceneResult)
			return std::unexpected(sceneResult.error());
		aiScene const* scene{ *sceneResult };
		std::string loadedName{ scene->mMeshes[0]->mName.C_Str() };
		std::vector<Mesh> loadedMeshes{};
		auto meshesResult{ loadMeshes(*scene, loadedMeshes) };
		if (!meshesResult)
			return std::unexpected(meshesResult.error());
		std::vector<std::string> loadedTextureNames{};
		loadTextureNames(*scene, loadedTextureNames);

		name.swap(loadedName);
		meshes.swap(loadedMeshes);
		textureNames.swap(loadedTextureNames);

#ifdef NDEBUG
		std::string{}.swap(modelAbsolutePath);
#endif
		return {};
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
