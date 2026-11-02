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

	void TextureD3D12::Initialize()
	{
		if (auto const result{ Texture::Initialize() }; !result)
			throw AssetLoadException(result.error().message);
	}

} // namespace gg
