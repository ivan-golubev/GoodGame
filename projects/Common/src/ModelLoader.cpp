module;
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <DirectXMath.h>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;

using DirectX::XMFLOAT2;
using gg::Mesh;

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
	ModelLoader::ModelLoader()
	{
	}

	ModelLoader::~ModelLoader()
	{
	}

	std::unique_ptr<Model> ModelLoader::LoadModel(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram> shader)
	{
		std::string modelFileAbsPath{ std::filesystem::absolute(modelRelativePath).generic_string() };
		std::unique_ptr<Model> model = std::make_unique<Model>();
		model->shaderProgram = std::move(shader);

		if (!LoadMeshes(modelFileAbsPath, *model))
			throw AssetLoadException(std::format("Failed to read the input model: {}", modelFileAbsPath));
		return model;
	}

	bool ModelLoader::LoadMeshes(std::string const& modelAbsolutePath, Model& outModel)
	{
		Assimp::Importer importer;
		aiScene const* scene = importer.ReadFile(modelAbsolutePath,
			aiProcess_Triangulate
			| aiProcess_JoinIdenticalVertices
			| aiProcess_SortByPType
			| aiProcess_FlipUVs
		);
		if (!scene)
			throw AssetLoadException(std::format("Failed to read the input model: {}, error: {}", modelAbsolutePath, importer.GetErrorString()));
		for (unsigned int i{ 0 }; i < scene->mNumMeshes; ++i)
			outModel.meshes.emplace_back(readMesh(scene->mMeshes[i], scene));
		return true;
	}

} // namespace gg