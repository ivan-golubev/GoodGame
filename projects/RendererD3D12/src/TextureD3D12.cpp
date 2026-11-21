module;
#include <array>
#include <directx/d3d12.h>
#include <format>
#include <string>
#include <utility>
module TextureD3D12;

import DeviceContextD3D12;
import ErrorHandling;
import Texture;

namespace gg
{
	TextureD3D12::TextureD3D12(std::string const& textureRelativePath)
		: Texture{ textureRelativePath }
	{
	}

	std::expected<void, Error> TextureD3D12::Initialize(DeviceContextD3D12& context, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
	{
		if (auto const result{ Texture::Initialize() }; !result)
			return std::unexpected(result.error());

		std::array<Texture const*, 1> const images{ this };
		auto                                uploadResult{ context.UploadTexture(
			images,
			D3D12_SRV_DIMENSION_TEXTURE2D,
			srvHandle,
			GetName(),
			textureAbsolutePath
		) };
		if (!uploadResult)
			return std::unexpected(std::move(uploadResult.error()));
		texture = std::move(*uploadResult);
		return {};
	}

} // namespace gg
