module;
#include <array>
#include <string>
#include <vector>
export module Texture;

export namespace gg
{
	using SkyboxTextures = std::array<std::string, 6>;

	class Texture
	{
	public:
		Texture(std::string const& relativePath);
		Texture(SkyboxTextures const& relativePaths);
		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;

		Texture(Texture&&) noexcept;
		Texture& operator=(Texture&&) noexcept;

		virtual ~Texture() noexcept;

		uint64_t SizeBytes() const;
		uint64_t LayerSizeBytes() const;

		std::wstring GetName();

		std::string const absPath;
		uint32_t width{};
		uint32_t height{};
		uint32_t channels{};
		std::vector<uint8_t*> imageData{};

	private:
		void swap(Texture&&) noexcept;
	};

	SkyboxTextures GetSkyboxTexturePaths(std::string const& name);

} // namespace gg
