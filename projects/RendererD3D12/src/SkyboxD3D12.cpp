module;
#include <array>
#include <cstdint>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <expected>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <wrl.h>
module SkyboxD3D12;

import D3DHelpers;
import ErrorHandling;
import RendererD3D12;
import ShaderProgram;
import ShaderProgramD3D12;
import Skybox;
import Texture;

namespace gg
{
	SkyboxD3D12::SkyboxD3D12(std::string const& name, std::shared_ptr<ShaderProgram> s, ID3D12Device* device)
		: Skybox{ name, s }
		, name{ name }
		, device{ device }
	{
	}

	std::expected<void, Error> SkyboxD3D12::Initialize()
	{
		if (auto result{ Skybox::Initialize() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateCubeTexture() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateRootSignature() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreatePipeline() }; !result)
			return std::unexpected(result.error());
		return {};
	}

	std::expected<void, Error> SkyboxD3D12::CreateCubeTexture()
	{
		std::wstring const nameW{ name.begin(), name.end() };

		D3D12_DESCRIPTOR_HEAP_DESC const srvHeapDesc{
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		};
		HRESULT const heapResult{ device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)) };
		if (FAILED(heapResult))
			return std::unexpected(Error{
				.code = ErrorCode::D3D12InitializationFailed,
				.message = std::format("Failed to create the skybox SRV descriptor heap: HRESULT 0x{:08X}", static_cast<uint32_t>(heapResult)),
			});
		SetName(srvHeap.Get(), std::format(L"SRV_DescriptorHeap_Skybox_{}", nameW));

		std::array<Texture const*, faceCount> faces{};
		for (size_t i{}; i < faceCount; ++i)
			faces[i] = textures[i].get();

		auto uploadResult{ RendererD3D12::Get()->UploadTexture(
			faces
			, D3D12_SRV_DIMENSION_TEXTURECUBE
			, srvHeap->GetCPUDescriptorHandleForHeapStart()
			, std::format(L"Texture_Skybox_{}", nameW)
			, std::format("skybox '{}'", name)) };
		if (!uploadResult)
			return std::unexpected(std::move(uploadResult.error()));
		cubeTexture = std::move(*uploadResult);
		return {};
	}

	// Resources read by skybox.hlsl. Register numbers match its register(...) annotations.
	std::expected<void, Error> SkyboxD3D12::CreateRootSignature()
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{
			.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1,
		};
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

		/* No vertex input: the fullscreen triangle is generated from SV_VertexID. Only the pixel shader reads resources. */
		D3D12_ROOT_SIGNATURE_FLAGS const rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		/* t0: the cube map SRV */
		CD3DX12_DESCRIPTOR_RANGE1 cubeMapRange;
		cubeMapRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		/* b0: SkyboxCameraCB as root constants (32 DWORDs), used to rebuild the view ray */
		rootParameters[0].InitAsConstants(sizeof(SkyboxCameraCB) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &cubeMapRange, D3D12_SHADER_VISIBILITY_PIXEL);

		/* s0: linear filtering; clamp so nothing wraps across a face edge */
		D3D12_STATIC_SAMPLER_DESC const sampler{
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			.MipLODBias = 0,
			.MaxAnisotropy = 0,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
			.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
			.MinLOD = 0.0f,
			.MaxLOD = D3D12_FLOAT32_MAX,
			.ShaderRegister = 0,
			.RegisterSpace = 0,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT const serializeResult{ D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob) };
		if (FAILED(serializeResult))
		{
			std::string const details{ errorBlob ? std::string{ static_cast<char const*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize() } : std::string{} };
			return std::unexpected(Error{
				.code = ErrorCode::D3D12InitializationFailed,
				.message = std::format("Failed to serialize the skybox root signature: HRESULT 0x{:08X} {}", static_cast<uint32_t>(serializeResult), details),
			});
		}

		HRESULT const createResult{ device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)) };
		if (FAILED(createResult))
			return std::unexpected(Error{
				.code = ErrorCode::D3D12InitializationFailed,
				.message = std::format("Failed to create the skybox root signature: HRESULT 0x{:08X}", static_cast<uint32_t>(createResult)),
			});
		std::wstring const nameW{ name.begin(), name.end() };
		SetName(rootSignature.Get(), std::format(L"RootSignature_Skybox_{}", nameW));
		return {};
	}

	std::expected<void, Error> SkyboxD3D12::CreatePipeline()
	{
		BreakIfFalse(shaderProgram.get());
		ShaderProgramD3D12 const* const shader{ dynamic_cast<ShaderProgramD3D12 const*>(shaderProgram.get()) };
		BreakIfFalse(shader);

		/* The skybox is a single fullscreen triangle generated in the vertex shader from SV_VertexID,
		   so it reads no vertex attributes: empty input layout, and no vertex buffer to bind.
		   Draw it with DrawInstanced(3, 1, 0, 0). */
		D3D12_INPUT_LAYOUT_DESC const inputLayout{
			.pInputElementDescs = nullptr,
			.NumElements = 0,
		};

		/* The shader writes z = 1 (far plane). Test against the depth buffer so models cover the sky,
		   with LESS_EQUAL so the sky passes where the buffer is still at its cleared value of 1.
		   No depth writes: the sky never occludes anything. */
		CD3DX12_DEPTH_STENCIL_DESC1 depthStencil{ D3D12_DEFAULT };
		depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		std::wstring const nameW{ name.begin(), name.end() };
		auto result{ RendererD3D12::Get()->CreateGraphicsPipeline(
			CD3DX12_SHADER_BYTECODE(shader->GetVertexShader().Get())
			, CD3DX12_SHADER_BYTECODE(shader->GetFragmentShader().Get())
			, inputLayout
			, D3D12_CULL_MODE_NONE /* one triangle that always faces the viewer; nothing to cull */
			, depthStencil
			, rootSignature.Get()
			, std::format(L"PSO_Skybox_{}", nameW)) };
		if (!result)
			return std::unexpected(std::move(result.error()));
		pipelineState = std::move(*result);
		return {};
	}
} // namespace gg
