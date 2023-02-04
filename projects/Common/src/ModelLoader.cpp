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
#include <tiny_gltf.h>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;

using DirectX::XMFLOAT2;
using gg::Mesh;
using gg::Model;
using gg::AssetLoadException;
using gg::DebugLog;
using gg::DebugLevel;
using gg::BreakIfFalse;

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

	bool LoadMeshesAssimp(std::string const& modelAbsolutePath, Model& outModel)
	{
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
		for (unsigned int i{ 0 }; i < scene->mNumMeshes; ++i)
			outModel.meshes.emplace_back(readMesh(scene->mMeshes[i], scene));
		return true;
	}

	bool LoadMeshesTinyGltf(std::string const& modelAbsolutePath, Model& outModel)
	{
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		/* assume the input model is GLTF 2.0 binary (.glb) */
		BreakIfFalse(modelAbsolutePath.ends_with(".glb"));
		bool loadResult = loader.LoadBinaryFromFile(&model, &err, &warn, modelAbsolutePath);
		if (!warn.empty())
			DebugLog(DebugLevel::Error, warn);

		if (!err.empty())
			DebugLog(DebugLevel::Error, err);

		if (!loadResult)
		{
			DebugLog(DebugLevel::Error, "Failed to parse glTF\n");
			return false;
		}
		/* At least one mesh should be in the model */
		BreakIfFalse(model.meshes.size() > 0);
		tinygltf::Mesh& gltfMesh = model.meshes[0];

		/* At least one primitive should be in the mesh */
		BreakIfFalse(gltfMesh.primitives.size() > 0);
		tinygltf::Primitive& primitive = gltfMesh.primitives[0];

		/* Expecting two attributes - position and texture coordinates */
		BreakIfFalse(primitive.attributes.size() >= 2);
		int positionAccessorIx = primitive.attributes.at("POSITION");
		int texcoordAccessorIx = primitive.attributes.at("TEXCOORD_0");

		tinygltf::Accessor& positionAccessor = model.accessors[positionAccessorIx];
		tinygltf::Accessor& texcoordAccessor = model.accessors[texcoordAccessorIx];
		/* expect the same number of positions and tex coordinates == vertex count */
		BreakIfFalse(positionAccessor.count == texcoordAccessor.count);

		int positionBufferIx = model.bufferViews[positionAccessor.bufferView].buffer;
		int texcoordBufferIx = model.bufferViews[texcoordAccessor.bufferView].buffer;

		/* expect positions and texture coordinates in one buffer */
		BreakIfFalse(positionBufferIx == texcoordBufferIx);
		tinygltf::Buffer& buffer = model.buffers[positionBufferIx];

		int offsetBytes = (3 + 2) * sizeof(float); // each vertex has xyz position and uv texture coordinates = 5 floats
		Mesh outMesh{};
		for (int i = 0; i < positionAccessor.count; ++i)
		{
			int offset = i * offsetBytes;
			float x, y, z, u, v;
			memcpy(&x, &buffer.data[offset], sizeof(float));
			memcpy(&y, &buffer.data[offset + sizeof(float)], sizeof(float));
			memcpy(&z, &buffer.data[offset + 2 * sizeof(float)], sizeof(float));
			memcpy(&u, &buffer.data[offset + 3 * sizeof(float)], sizeof(float));
			memcpy(&v, &buffer.data[offset + 4 * sizeof(float)], sizeof(float));
			outMesh.vertices.emplace_back(
				x,
				y,
				z,
				1.0f, // w
				u,
				v
			);
		}
		outModel.meshes.emplace_back(std::move(outMesh));
		//gltfMesh.name
		return true;
	}
}

namespace gg
{
	void LoadMeshes(std::string const& modelRelativePath, Model& model)
	{
		std::string modelFileAbsPath{ std::filesystem::absolute(modelRelativePath).generic_string() };
		//if (!LoadMeshesAssimp(modelFileAbsPath, model))
		if (!LoadMeshesTinyGltf(modelFileAbsPath, model))
			throw AssetLoadException(std::format("Failed to read the input model: {}", modelFileAbsPath));
	}

} // namespace gg