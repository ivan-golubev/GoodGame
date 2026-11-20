module;
#include <filesystem>
#include <format>
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

	void Texture::PixelsDeleter::operator()(unsigned char* data) const noexcept
	{
		stbi_image_free(data);
	}

	std::expected<void, Error> Texture::Initialize()
	{
		pixels.reset();
		width    = 0;
		height   = 0;
		channels = 0;
		int imageWidth{}, imageHeight{}, imageChannels{};
		pixels.reset(stbi_load(textureAbsolutePath.c_str(), &imageWidth, &imageHeight, &imageChannels, STBI_rgb_alpha));
		if (!pixels)
		{
			auto const message{ std::format(
				"Failed to load texture '{}': {}",
				textureAbsolutePath,
				stbi_failure_reason() ? stbi_failure_reason() : "unknown image loading error"
			) };
			return std::unexpected(Error{
				.code    = ErrorCode::TextureLoadFailed,
				.message = message,
			});
		}
		width    = static_cast<uint32_t>(imageWidth);
		height   = static_cast<uint32_t>(imageHeight);
		channels = static_cast<uint32_t>(imageChannels);
#ifdef NDEBUG
		std::string{}.swap(textureAbsolutePath);
#endif
		return {};
	}

	std::wstring Texture::GetName()
	{
		std::wstring baseName{ std::filesystem::path(textureAbsolutePath).stem() };
		return L"Texture_" + baseName;
	}

	Texture::~Texture() noexcept = default;

	uint64_t Texture::SizeBytes() const
	{
		return static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * bytesPerPixel;
	}
} // namespace gg
