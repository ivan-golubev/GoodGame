module;
#include <string>
#include <expected>
export module Texture;

export import ErrorHandling;

namespace gg
{
	export class Texture
	{
	public:
		virtual ~Texture();
		explicit Texture(std::string const& textureRelativePath);
		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;
		std::expected<void, Error> Initialize();
		uint64_t SizeBytes() const;

		std::wstring GetName();

		std::string textureAbsolutePath;
		uint32_t width{};
		uint32_t height{};
		uint32_t channels{};
		unsigned char* pixels{};

	};
} // namespace gg
