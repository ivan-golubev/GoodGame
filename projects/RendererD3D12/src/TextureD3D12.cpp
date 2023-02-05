module;
#include <string>
module TextureD3D12;

import Texture;
import ErrorHandling;
import RendererD3D12;

namespace gg
{
	TextureD3D12::TextureD3D12(std::string const& textureRelativePath)
		: Texture(textureRelativePath)
	{
	}

} // namespace gg