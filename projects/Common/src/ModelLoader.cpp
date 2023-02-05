module;
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <DirectXMath.h>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;
import Application;
import Renderer;

using DirectX::XMFLOAT2;
using gg::Mesh;
using gg::Model;
using gg::AssetLoadException;

namespace
{
	void readVertices(aiMesh const* assimpMesh, Mesh& outMesh, uint32_t UVsetNumber)
	{
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

				outMesh.vertices.emplace_back(
					static_cast<float>(assimpVertex.x),
					static_cast<float>(assimpVertex.y),
					static_cast<float>(assimpVertex.z),
					1.0f, // w
					UV.x, UV.y
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
		/* Load meshes */
		for (unsigned int i{ 0 }; i < scene->mNumMeshes; ++i)
			model.meshes.emplace_back(readMesh(scene->mMeshes[i], scene));
		/* Load textures */
		std::shared_ptr<Renderer> renderer{ Application::Get()->GetRenderer() };
		for (uint32_t i = 0; i < scene->mNumTextures; ++i)
			model.textures.emplace_back(
				renderer->LoadTexture(scene->mTextures[i]->mFilename.C_Str())
			);
	}

} // namespace gg