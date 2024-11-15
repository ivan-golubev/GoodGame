module;
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <filesystem>
#include <format>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;

using gg::Mesh;
using gg::Model;
using gg::AssetLoadException;
using gg::BreakIfFalse;

namespace
{
	void readVertices(aiMesh const* assimpMesh, Mesh& outMesh, uint32_t UVsetNumber)
	{
		BreakIfFalse(assimpMesh->HasPositions());
		BreakIfFalse(assimpMesh->HasTextureCoords(UVsetNumber));
		BreakIfFalse(assimpMesh->HasNormals());

		for (unsigned int faceIndex{ 0 }; faceIndex < assimpMesh->mNumFaces; ++faceIndex)
		{
			aiFace* face{ &assimpMesh->mFaces[faceIndex] };
			for (unsigned int j{ 0 }; j < face->mNumIndices; ++j)
			{
				auto vertexIndex = face->mIndices[j];
				auto assimpVertex = assimpMesh->mVertices[vertexIndex];

				aiVector3D UV = assimpMesh->HasTextureCoords(UVsetNumber)
					? assimpMesh->mTextureCoords[UVsetNumber][vertexIndex]
					: aiVector3D{ 0, 0, 0 };

				aiVector3D normal = assimpMesh->mNormals[vertexIndex];

				outMesh.vertices.emplace_back(
					static_cast<float>(assimpVertex.x),
					static_cast<float>(assimpVertex.y),
					static_cast<float>(assimpVertex.z),
					1.0f, // w
					UV.x, UV.y,
					normal.x, normal.y, normal.z, 0.0f
				);
			}
		}
	}

	Mesh readMesh(aiMesh const* assimpMesh, aiScene const* scene)
	{
		Mesh mesh{};
		readVertices(assimpMesh, mesh, 0);
		return mesh;
	}
}

namespace gg
{
	void LoadData(std::string const& modelRelativePath, Model& model)
	{
		std::string modelAbsolutePath{ std::filesystem::absolute(modelRelativePath).generic_string() };
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
		model.name = scene->mMeshes[0]->mName.C_Str();
		/* Load meshes */
		for (unsigned int i{ 0 }; i < scene->mNumMeshes; ++i)
			model.meshes.emplace_back(readMesh(scene->mMeshes[i], scene));
		/* Save texture names */
		for (uint32_t i = 0; i < scene->mNumTextures; ++i)
			model.textureNames.emplace_back(scene->mTextures[i]->mFilename.C_Str());
	}

} // namespace gg