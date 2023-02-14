module;
#include <filesystem>
#include <stb_image.h>
#include <string>
module Texture;

namespace gg
{
	Texture::Texture(std::string const& relativePath)
		: absPath{ std::filesystem::absolute(relativePath).generic_string() }
	{
		int width, height, channels;
		pixels = stbi_load(absPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		this->width = width;
		this->height = height;
		this->channels = channels;
	}

	std::wstring Texture::GetName()
	{
		std::wstring baseName{ std::filesystem::path(absPath).stem() };
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
