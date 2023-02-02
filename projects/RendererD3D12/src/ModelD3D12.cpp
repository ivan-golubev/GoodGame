module;
#include <string>
#include <memory>
module ModelD3D12;

import Model;

namespace gg
{
	ModelD3D12::ModelD3D12(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram> s, std::shared_ptr<Texture> t)
		: Model{ modelRelativePath, std::move(s), t }
	{


		// TODO: this is no longer needed, we load from a model now.
		// but need to extract the command list stuff here

		//void RendererD3D12::UploadGeometry()
		//{
		//	/* Initialize the vertices. TODO: move to a separate class */
		//	// TODO: in fact, cubes are not fun, read data from an .fbx
		//	std::vector<Vertex> const vertices{
		//		/*  x      y      z     w     r      g    b     a */
		//		{-1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 0
		//		{-1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f}, // 1
		//		{ 1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f}, // 2
		//		{ 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f}, // 3
		//		{-1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 4
		//		{-1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f}, // 5
		//		{ 1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f}, // 6
		//		{ 1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f}, // 7
		//	};
		//	std::vector<uint32_t> const indices{
		//		0, 1, 2, 0, 2, 3,
		//		4, 6, 5, 4, 7, 6,
		//		4, 5, 1, 4, 1, 0,
		//		3, 2, 6, 3, 6, 7,
		//		1, 5, 6, 1, 6, 2,
		//		4, 0, 3, 4, 3, 7
		//	};
		//	indexCount = static_cast<uint32_t>(indices.size());
		//}
	}

	ModelD3D12::~ModelD3D12() noexcept
	{

	}

} //namespace gg
