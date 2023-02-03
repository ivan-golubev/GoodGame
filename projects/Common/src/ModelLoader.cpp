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
#include <cgltf/cgltf.h>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;

using DirectX::XMFLOAT2;
using gg::Mesh;
using gg::Model;
using gg::AssetLoadException;
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

	bool LoadMeshesCGLTF(std::string const& modelAbsolutePath, Model& outModel)
	{
		cgltf_options options{ 0 };
		cgltf_data* data{ nullptr };

		cgltf_result result = cgltf_parse_file(&options, modelAbsolutePath.c_str(), &data);
		if (result != cgltf_result_success)
			return false;

		//result = cgltf_load_buffers(&options, data, data->meshes[0].name);
		//if (result != cgltf_result_success)
		//	return false;

		result = cgltf_validate(data);
		if (result != cgltf_result_success)
			return false;

		// TODO: wow, we can read the attributes from the .glb files too, sick !
		// TODO: read the name of the mesh too, why not
		// TODO: we can actually read transforms of each game object from .glb and export whole scenes from Blender, awesome !

		/* expect at least one mesh in the model */
		BreakIfFalse(data->meshes_count > 0);
		cgltf_mesh* mesh = &data->meshes[0];

		/* expect at least one primitive in the mesh */
		BreakIfFalse(mesh->primitives_count > 0 && mesh->primitives[0].type == cgltf_primitive_type_triangles);
		cgltf_primitive* firstPrimitive = &mesh->primitives[0];

		/* expect exactly two attributes in the primitive - position and texture coordinates */
		BreakIfFalse(firstPrimitive->attributes_count == 2);
		BreakIfFalse(firstPrimitive->attributes[0].type == cgltf_attribute_type_position);
		BreakIfFalse(firstPrimitive->attributes[1].type == cgltf_attribute_type_texcoord);
		cgltf_attribute* position = &firstPrimitive->attributes[0];
		cgltf_attribute* textureCoordinates = &firstPrimitive->attributes[1];

		/* expect the count of positions matches the count of texture coordinates */
		cgltf_size const vertexCount{ position->data->count };
		BreakIfFalse(vertexCount == textureCoordinates->data->count);

		// K, need to load buffers for positions and tex coordinates
		// but I don't have an URI ? it is null
		//textureCoordinates->data->buffer_view->buffer[]

		Mesh outMesh{};
		for (int i = 0; i < vertexCount; ++i)
		{
			//outMesh.vertices.emplace_back(
			//	static_cast<float>(assimpVertex.x),
			//	static_cast<float>(assimpVertex.y),
			//	static_cast<float>(assimpVertex.z),
			//	1.0f, // w
			//	UV.x, UV.y
			//);
		}
		outModel.meshes.emplace_back(std::move(outMesh));

		cgltf_free(data);
		return true;
	}
}

namespace gg
{
	void LoadMeshes(std::string const& modelRelativePath, Model& model)
	{
		std::string modelFileAbsPath{ std::filesystem::absolute(modelRelativePath).generic_string() };
		//if (!LoadMeshesAssimp(modelFileAbsPath, model))
		if (!LoadMeshesCGLTF(modelFileAbsPath, model))
			throw AssetLoadException(std::format("Failed to read the input model: {}", modelFileAbsPath));
	}

} // namespace gg