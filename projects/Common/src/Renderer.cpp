module;
#include <cstdint>
#include <SDL2/SDL_video.h>
module Renderer;

namespace gg 
{
	Renderer::Renderer(uint32_t width, uint32_t height, SDL_Window* windowHandle)
		: mWidth{ width }
		, mHeight{ height }
		, mWindowHandle{ windowHandle }
	{
	}
}