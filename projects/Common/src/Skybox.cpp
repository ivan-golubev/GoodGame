module;
#include <format>
#include <string>
module Skybox;

import SettingsRenderer;

namespace
{
	inline std::string SkyBoxTexturePath(std::string name, std::string part)
	{
		return std::format("{}/skybox/{}/{}.{}", gg::texturesLocation, name, part, gg::texturesExtension);
	}
} // namespace

namespace gg
{
	Skybox::Skybox(std::string name)
		: textures{
			SkyBoxTexturePath(name, "left"),
			SkyBoxTexturePath(name, "top"),
			SkyBoxTexturePath(name, "front"),
			SkyBoxTexturePath(name, "bottom"),
			SkyBoxTexturePath(name, "right"),
			SkyBoxTexturePath(name, "back")
		}
	{
	};
} // namespace gg
