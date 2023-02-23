module;
#include <array>
#include <filesystem>
#include <format>
#include <stb_image.h>
#include <string>
module Texture;

import ErrorHandling;
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
	Texture::Texture(std::string const& relativePath)
		: absPath{ std::filesystem::absolute(relativePath).generic_string() }
	{
		int width, height, channels;
		uint8_t* pixels = stbi_load(absPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		imageData.push_back(pixels);
		this->width = width;
		this->height = height;
		this->channels = channels;
	}

	SkyboxTextures GetSkyboxTexturePaths(std::string const& name)
	{
		SkyboxTextures texturePaths
		{
			SkyBoxTexturePath(name, "left"),
			SkyBoxTexturePath(name, "top"),
			SkyBoxTexturePath(name, "front"),
			SkyBoxTexturePath(name, "bottom"),
			SkyBoxTexturePath(name, "right"),
			SkyBoxTexturePath(name, "back")
		};
		return texturePaths;
	}

	Texture::Texture(SkyboxTextures const& relativePaths)
		: absPath{ std::filesystem::absolute(relativePaths[0]).generic_string() }
	{
		int width, height, channels;
		for (std::string const& texPath : relativePaths)
		{
			uint8_t* pixels = stbi_load(texPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			imageData.push_back(pixels);
		}
		this->width = width;
		this->height = height;
		this->channels = channels;

		BreakIfFalse(SizeBytes() > 0);
		BreakIfFalse(channels == 4); /* expecting rgba textures */
		BreakIfFalse(imageData.size() == 6); /* a cube map should have exactly 6 textures */
		BreakIfFalse(LayerSizeBytes() * 6 == SizeBytes()); /* all textures must have the same size */
		BreakIfFalse(width == height); /* each texture must be a square */
	}

	Texture::Texture(Texture&& other) noexcept
	{
		swap(std::forward<Texture>(other));
	}

	Texture& Texture::operator=(Texture&& other) noexcept
	{
		if (this != &other)
			swap(std::forward<Texture>(other));
		return *this;
	}

	void Texture::swap(Texture&& other) noexcept
	{
		std::swap(imageData, other.imageData);
		std::swap(width, other.width);
		std::swap(height, other.height);
		std::swap(channels, other.channels);
	}

	Texture::~Texture()
	{
		for (uint8_t* image : imageData)
			stbi_image_free(image);
	}

	std::wstring Texture::GetName()
	{
		std::wstring baseName{ std::filesystem::path(absPath).stem() };
		return L"Texture_" + baseName;
	}

	uint64_t Texture::SizeBytes() const
	{
		return static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels * imageData.size();
	}

	uint64_t Texture::LayerSizeBytes() const
	{
		return static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels;
	}
} // namespace gg
