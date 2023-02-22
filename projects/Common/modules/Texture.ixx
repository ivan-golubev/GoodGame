module;
#include <string>
export module Texture;

namespace gg
{
	export class Texture
	{
	public:
		Texture(std::string const& relativePath);
		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;

		Texture(Texture&&) noexcept;
		Texture& operator=(Texture&&) noexcept;

		virtual ~Texture() noexcept;

		uint64_t SizeBytes() const;

		std::wstring GetName();

		std::string const absPath;
		uint32_t width{};
		uint32_t height{};
		uint32_t channels{};
		uint8_t* pixels{};

	private:
		void swap(Texture&&) noexcept;
	};

} // namespace gg
