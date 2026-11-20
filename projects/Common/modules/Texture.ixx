module;
#include <expected>
#include <string>
export module Texture;

export import ErrorHandling;

namespace gg
{
	export class Texture
	{
	public:
		virtual ~Texture() noexcept;
		explicit Texture(std::string const& textureRelativePath);
		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;
		std::expected<void, Error> Initialize();
		uint64_t SizeBytes() const;

		static constexpr uint32_t bytesPerPixel{ 4 }; /* pixels are always loaded as RGBA8 */

		std::wstring GetName();

		std::string textureAbsolutePath;
		uint32_t width{};
		uint32_t height{};
		uint32_t channels{}; /* channel count of the source file; pixels in memory are always bytesPerPixel wide */
		unsigned char* pixels{};
	};
} // namespace gg
