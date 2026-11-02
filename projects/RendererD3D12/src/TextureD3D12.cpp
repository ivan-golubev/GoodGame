module;
#include <string>
#include <format>
#include <utility>
module TextureD3D12;

import Texture;
import ErrorHandling;
import RendererD3D12;

namespace gg
{
	TextureD3D12::TextureD3D12(std::string const& textureRelativePath)
		: Texture{ textureRelativePath }
	{
	}

	std::expected<void, Error> TextureD3D12::Initialize()
	{
		if (auto const result{ Texture::Initialize() }; !result)
			return std::unexpected(result.error());
		return {};
	}

} // namespace gg
