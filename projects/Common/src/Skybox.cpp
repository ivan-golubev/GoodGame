module;
#include <array>
#include <format>
#include <memory>
#include <string>
module Skybox;

import ErrorHandling;
import SettingsRenderer;
import ShaderProgram;
import Texture;

namespace gg
{
	namespace
	{
		// Cube map layers are ordered +X, -X, +Y, -Y, +Z, -Z in both Vulkan (VkImageSubresourceRange)
		// and D3D12 (TextureCube array slices).
		constexpr std::array<char const*, Skybox::faceCount> skyboxFaceNames{ "right", "left", "top", "bottom", "front", "back" };
	}

	Skybox::Skybox(std::string const& name, std::shared_ptr<ShaderProgram> s)
		: shaderProgram{ s }
	{
		for (size_t i{}; i < textures.size(); ++i)
			textures[i] = std::make_unique<Texture>(std::format(
				"{}/skybox/{}/{}.{}",
				texturesLocation,
				name,
				skyboxFaceNames[i],
				texturesExtension
			));
	}

	std::expected<void, Error> Skybox::Initialize()
	{
		for (auto& texture : textures)
		{
			if (auto result{ texture->Initialize() }; !result)
				return std::unexpected(result.error());
		}
		for (size_t i{ 1 }; i < textures.size(); ++i)
		{
			if (textures[i]->width != textures[0]->width || textures[i]->height != textures[0]->height)
				return std::unexpected(Error{
					.code    = ErrorCode::TextureLoadFailed,
					.message = "Skybox faces must all have the same dimensions.",
				});
		}
		if (textures[0]->width != textures[0]->height)
			return std::unexpected(Error{ .code = ErrorCode::TextureLoadFailed, .message = "Skybox face textures must be square." });
		return {};
	}
} // namespace gg
