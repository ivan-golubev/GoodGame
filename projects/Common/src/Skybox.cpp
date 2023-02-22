module;
#include <format>
#include <string>
module Skybox;

import SettingsRenderer;
import ErrorHandling;

namespace
{
	inline std::string SkyBoxTexturePath(std::string name, std::string part)
	{
		return std::format("{}/skybox/{}/{}.{}", gg::texturesLocation, name, part, gg::texturesExtension);
	}
} // namespace

namespace gg
{
	Skybox::Skybox(std::string const& name)
	{
		textures.emplace_back(SkyBoxTexturePath(name, "left"));
		textures.emplace_back(SkyBoxTexturePath(name, "top"));
		textures.emplace_back(SkyBoxTexturePath(name, "front"));
		textures.emplace_back(SkyBoxTexturePath(name, "bottom"));
		textures.emplace_back(SkyBoxTexturePath(name, "right"));
		textures.emplace_back(SkyBoxTexturePath(name, "back"));

		dataSize = textures[0].width * textures[0].height * textures[0].channels * textures.size();

		BreakIfFalse(dataSize > 0);
		BreakIfFalse(textures[0].channels == 4); /* expecting rgba textures */
		BreakIfFalse(textures.size() == 6); /* a cube map should have exactly 6 textures */
		BreakIfFalse(GetLayerSizeBytes() * 6 == GetDataSizeBytes()); /* all textures must have the same size */
		BreakIfFalse(textures[0].width == textures[0].height); /* each texture must be a square */
	}

	uint64_t Skybox::GetDataSizeBytes() const
	{
		return dataSize;
	}

	uint64_t Skybox::GetLayerSizeBytes() const
	{
		return dataSize / textures.size();
	}

} // namespace gg
