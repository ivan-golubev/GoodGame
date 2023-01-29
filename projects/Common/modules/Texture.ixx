module;
#include <string>
export module Texture;

namespace gg
{
	export class Texture
	{
	public:
		Texture(std::string const& relativePath);
		virtual ~Texture();
		uint64_t SizeBytes() const;

		std::string const absPath;
		uint32_t width;
		uint32_t height;
		uint32_t channels;
		unsigned char* pixels;

	};
} // namespace gg
