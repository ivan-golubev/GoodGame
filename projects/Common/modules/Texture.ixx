module;
#include <string>
export module Texture;

namespace gg
{
	export struct Texture
	{
	public:
		Texture(std::string const& relativePath);
		~Texture();
		uint64_t SizeBytes() const;

		std::string const absPath;
		uint32_t width;
		uint32_t height;
		uint32_t channels;
		unsigned char* pixels;

	};
} // namespace gg
