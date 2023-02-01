module;
#include <memory>
module ModelD3D12;

import Model;

namespace gg
{
	ModelD3D12::ModelD3D12(std::unique_ptr<ShaderProgram> s, std::shared_ptr<Texture> t)
		: Model{ std::move(s), t }
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

		//	ThrowIfFailed(mCommandAllocator->Reset());
		//	ThrowIfFailed(commandList->Reset(mCommandAllocator.Get(), pipelineState.Get()));

		//	uint32_t const VB_sizeBytes = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
		//	uint32_t const IB_sizeBytes = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

		//	CreateBuffer(commandList, VB_GPU_Resource, VB_CPU_Resource, vertices.data(), VB_sizeBytes, L"VertexBuffer");
		//	CreateBuffer(commandList, IB_GPU_Resource, IB_CPU_Resource, indices.data(), IB_sizeBytes, L"IndexBuffer");

		//	ThrowIfFailed(commandList->Close());

		//	/* Upload Vertex and Index buffers */
		//	ID3D12CommandList* ppCommandLists[]{ commandList.Get() };
		//	mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		//	WaitForPreviousFrame();

		//	/* Init the Vertex/Index buffer views */
		//	vertexBufferView.BufferLocation = VB_GPU_Resource->GetGPUVirtualAddress();
		//	vertexBufferView.SizeInBytes = VB_sizeBytes;
		//	vertexBufferView.StrideInBytes = sizeof(Vertex);

		//	indexBufferView.BufferLocation = IB_GPU_Resource->GetGPUVirtualAddress();
		//	indexBufferView.SizeInBytes = IB_sizeBytes;
		//	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		//}
	}

	ModelD3D12::~ModelD3D12() noexcept
	{

	}
} //namespace gg
