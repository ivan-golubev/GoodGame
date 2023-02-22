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
		std::swap(pixels, other.pixels);
		std::swap(width, other.width);
		std::swap(height, other.height);
		std::swap(channels, other.channels);
	}

	Texture::~Texture()
	{
		if (pixels)
			stbi_image_free(pixels);
	}

	std::wstring Texture::GetName()
	{
		std::wstring baseName{ std::filesystem::path(absPath).stem() };
		return L"Texture_" + baseName;
	}

	uint64_t Texture::SizeBytes() const
	{
		return static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels;
	}
} // namespace gg
