module;
#include <filesystem>
#include <stb_image.h>
#include <string>
#include <utility>
module Texture;

namespace gg
{
	Texture::Texture(std::string const& textureRelativePath)
		: textureAbsolutePath{ std::filesystem::absolute(textureRelativePath).generic_string() }
	{
	}

	void Texture::Initialize()
	{
		stbi_image_free(pixels);
		int imageWidth{}, imageHeight{}, imageChannels{};
		pixels = stbi_load(textureAbsolutePath.c_str(), &imageWidth, &imageHeight, &imageChannels, STBI_rgb_alpha);
		width = static_cast<uint32_t>(imageWidth);
		height = static_cast<uint32_t>(imageHeight);
		channels = static_cast<uint32_t>(imageChannels);
#ifdef NDEBUG
		std::string{}.swap(textureAbsolutePath);
#endif
	}

	std::wstring Texture::GetName()
	{
		std::wstring baseName{ std::filesystem::path(textureAbsolutePath).stem() };
		return L"Texture_" + baseName;
	}

	Texture::~Texture()
	{
		stbi_image_free(pixels);
	}

	uint64_t Texture::SizeBytes() const
	{
		return static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4;
	}
} // namespace gg
