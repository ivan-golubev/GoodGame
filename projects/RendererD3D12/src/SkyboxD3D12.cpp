module;
#include <array>
#include <cstdint>
#include <directx/d3d12.h>
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
} // namespace gg
