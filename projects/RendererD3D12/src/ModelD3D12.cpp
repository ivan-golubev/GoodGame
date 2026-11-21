module;
#include "D3D12CheckHResult.h"
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <format>
#include <glm/mat4x4.hpp>
#include <memory>
#include <string>
#include <utility>
#include <wrl.h>
module ModelD3D12;

import Camera;
import D3DHelpers;
import DeviceContextD3D12;
import ErrorHandling;
import Lighting;
import Model;
import Renderer;
import SettingsRenderer;
import ShaderProgram;
import ShaderProgramD3D12;
import Texture;
import TextureD3D12;
import Vertex;

using Microsoft::WRL::ComPtr;

namespace gg
{
	ModelD3D12::ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s)
		: Model{ modelRelativePath, s }
	{
	}

	std::expected<void, Error> ModelD3D12::Initialize(DeviceContextD3D12& context, ComPtr<ID3D12RootSignature> modelRootSignature)
	{
		rootSignature = std::move(modelRootSignature);
		if (auto const result{ Model::Initialize() }; !result)
			return std::unexpected(result.error());
		if (auto const result{ CreateGraphicsPipeline(context) }; !result)
			return std::unexpected(result.error());
		if (auto const result{ CreateVertexBuffer(context) }; !result)
			return std::unexpected(result.error());
		if (auto const result{ CreateSRVHeap(context) }; !result)
			return std::unexpected(result.error());
		if (auto const result{ LoadTextures(context) }; !result)
			return std::unexpected(result.error());
		return {};
	}

	void ModelD3D12::Draw(
		ID3D12GraphicsCommandList* commandList,
		glm::mat4x4 const&         transform,
		Camera const&              camera,
		DirectionalLight const&    light
	)
	{
		commandList->SetPipelineState(pipelineState.Get());
		commandList->SetGraphicsRootSignature(rootSignature.Get());

		ID3D12DescriptorHeap* heaps[]{ srvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		ModelViewProjection const mvpMatrices{ CalculateModelViewProjection(transform, camera) };
		commandList->SetGraphicsRoot32BitConstants(0, sizeof(ModelViewProjection) / sizeof(float), &mvpMatrices, 0);
		commandList->SetGraphicsRoot32BitConstants(1, sizeof(DirectionalLight) / sizeof(float), &light, 0);
		commandList->SetGraphicsRootDescriptorTable(2, srvHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

		for (Mesh const& mesh : meshes)
			commandList->DrawInstanced(mesh.GetVertexCount(), 1, 0, 0);
	}

	std::expected<void, Error> ModelD3D12::CreateGraphicsPipeline(DeviceContextD3D12& context)
	{
		BreakIfFalse(shaderProgram.get());
		ShaderProgramD3D12 const* shaderD3D12{ dynamic_cast<ShaderProgramD3D12 const*>(shaderProgram.get()) };

		/* Specify the input layout */
		D3D12_INPUT_ELEMENT_DESC const inputLayout[]{
			{
				.SemanticName         = "POSITION",
				.SemanticIndex        = 0,
				.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.InputSlot            = 0,
				.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			},
			{
				.SemanticName         = "NORMAL",
				.SemanticIndex        = 0,
				.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.InputSlot            = 0,
				.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			},
			{
				.SemanticName         = "TEXCOORD",
				.SemanticIndex        = 0,
				.Format               = DXGI_FORMAT_R32G32_FLOAT,
				.InputSlot            = 0,
				.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			}
		};

		std::string const  psoName{ std::format("PSO_Model_{}", name) };
		std::wstring const psoNameW{ psoName.begin(), psoName.end() };

		auto result{ context.CreateGraphicsPipeline(
			CD3DX12_SHADER_BYTECODE(shaderD3D12->GetVertexShader().Get()),
			CD3DX12_SHADER_BYTECODE(shaderD3D12->GetFragmentShader().Get()),
			D3D12_INPUT_LAYOUT_DESC{ .pInputElementDescs = inputLayout, .NumElements = _countof(inputLayout) },
			D3D12_CULL_MODE_BACK,
			CD3DX12_DEPTH_STENCIL_DESC1{ D3D12_DEFAULT },
			rootSignature.Get(),
			psoNameW
		) };
		if (!result)
			return std::unexpected(std::move(result.error()));
		pipelineState = std::move(*result);
		return {};
	}

	std::expected<void, Error> ModelD3D12::CreateVertexBuffer(DeviceContextD3D12& context)
	{
		// TODO: handle multiple meshes properly, need to create multiple Vertex Buffers
		BreakIfFalse(meshes.size() < 2);
		Mesh const& mesh{ meshes[0] };

		uint64_t const vertexBufferSizeBytes{ mesh.VerticesSizeBytes() };
		if (auto result{ context.UploadBuffer(
				vertexBuffer,
				vertexBufferUpload,
				mesh.vertices.data(),
				vertexBufferSizeBytes,
				std::format(L"Vertex_Model_{}", std::wstring{ name.begin(), name.end() })
			) };
		    !result)
			return std::unexpected(result.error());

		/* Init the Vertex buffer view */
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes    = static_cast<uint32_t>(vertexBufferSizeBytes);
		vertexBufferView.StrideInBytes  = sizeof(Vertex);
		return {};
	}

	std::expected<void, Error> ModelD3D12::CreateSRVHeap(DeviceContextD3D12& context)
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
			.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		};
		HRESULT const d3d12CreateModelTextureDescriptorHeapResult{ context.GetDevice()->CreateDescriptorHeap(
			&srvHeapDesc,
			IID_PPV_ARGS(&srvHeap)
		) };
		D3D12_CHECK_HRESULT(
			d3d12CreateModelTextureDescriptorHeapResult,
			ErrorCode::D3D12InitializationFailed,
			"create model texture descriptor heap"
		);

		std::wstring const resourceName{ name.begin(), name.end() };
		SetName(srvHeap.Get(), std::format(L"SRV_DescriptorHeap_Model_{}", resourceName));
		return {};
	}

	std::expected<void, Error> ModelD3D12::LoadTextures(DeviceContextD3D12& context)
	{
		/* TODO: only one texture per model works. srvHeap is created with a single descriptor
		   (CreateSRVHeap, NumDescriptors = 1), and every texture writes its SRV to the start of the heap, so
		   each one overwrites the previous and only the last loaded texture is bound. The root signature
		   matches that: one SRV range of one descriptor at t0 (RendererD3D12::Initialize), and
		   textured_surface.hlsl samples a single diffuseTexture, so today's models are unaffected.
		   To support several textures per model:
		   - create srvHeap with textureNames.size() descriptors;
		   - write texture i at GetCPUDescriptorHandleForHeapStart() offset by
		     i * GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		   - widen the root signature's SRV range (or give each model a range of its own size) and declare
		     the extra textures (t1, t2, ...) in the shader, with a matching Vulkan descriptor layout. */
		for (std::string const& textureName : textureNames)
		{
			std::string const textureRelativePath{ std::format("{}/{}.{}", texturesLocation, textureName, texturesExtension) };
			auto              texture{ std::make_shared<TextureD3D12>(textureRelativePath) };
			if (auto const result{ texture->Initialize(context, srvHeap->GetCPUDescriptorHandleForHeapStart()) }; !result)
				return std::unexpected(result.error());
			textures.emplace_back(std::move(texture));
		}
		return {};
	}
} // namespace gg
