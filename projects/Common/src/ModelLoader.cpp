module;
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <format>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>
#include <unordered_map>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;

using gg::Mesh;
using gg::Model;
using gg::InputAttribute;
using gg::AssetLoadException;
using gg::Semantic;
using gg::BreakIfFalse;

namespace
{
	void readVertices(aiMesh const* assimpMesh, Mesh& outMesh, uint32_t UVsetNumber, std::unordered_map<Semantic, InputAttribute> const& inputAttributes)
	{
		bool loadNormals = false;
		bool loadTextureCoords = false;

		/* Check that the model is valid first */
		for (auto& [semantic, attr] : inputAttributes)
		{
			switch (semantic)
			{
			case Semantic::Position:
				BreakIfFalse(assimpMesh->HasPositions());
				break;
			case Semantic::Normal:
				BreakIfFalse(assimpMesh->HasNormals());
				loadNormals = true;
				break;
			case Semantic::TextureCoordinates:
				BreakIfFalse(assimpMesh->HasTextureCoords(UVsetNumber));
				loadTextureCoords = true;
				break;
			}
		}

		for (unsigned int faceIndex{ 0 }; faceIndex < assimpMesh->mNumFaces; ++faceIndex)
		{
			aiFace* face{ &assimpMesh->mFaces[faceIndex] };
			for (unsigned int j{ 0 }; j < face->mNumIndices; ++j)
			{
				auto vertexIndex = face->mIndices[j];
				auto assimpVertex = assimpMesh->mVertices[vertexIndex];

				outMesh.vertices.push_back(static_cast<float>(assimpVertex.x));
				outMesh.vertices.push_back(static_cast<float>(assimpVertex.y));
				outMesh.vertices.push_back(static_cast<float>(assimpVertex.z));
				if (inputAttributes.at(Semantic::Position).componentCount > 3)
					outMesh.vertices.push_back(1.0f); // w

				if (loadNormals)
				{
					aiVector3D normal = assimpMesh->mNormals[vertexIndex];
					outMesh.vertices.push_back(normal.x);
					outMesh.vertices.push_back(normal.y);
					outMesh.vertices.push_back(normal.z);
					if (inputAttributes.at(Semantic::Normal).componentCount > 3)
						outMesh.vertices.push_back(0.0f);
				}

				if (loadTextureCoords)
				{
					aiVector3D UV = assimpMesh->HasTextureCoords(UVsetNumber)
						? assimpMesh->mTextureCoords[UVsetNumber][vertexIndex]
						: aiVector3D{ 0, 0, 0 };
					outMesh.vertices.push_back(UV.x);
					outMesh.vertices.push_back(UV.y);
				}
			}
		}
	}

	Mesh readMesh(aiMesh const* assimpMesh, aiScene const* scene, std::unordered_map<Semantic, InputAttribute> const& inputAttributes)
	{
		Mesh mesh{};
		readVertices(assimpMesh, mesh, 0, inputAttributes);
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
			model.meshes.emplace_back(
				readMesh(scene->mMeshes[i], scene, model.shaderProgram->inputAttributes)
			);
		/* Save texture names */
		for (uint32_t i = 0; i < scene->mNumTextures; ++i)
			model.textureNames.emplace_back(scene->mTextures[i]->mFilename.C_Str());
	}

} // namespace gg