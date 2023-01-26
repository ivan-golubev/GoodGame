module;
#include <SDL2/SDL_video.h>
export module ApplicationSettings;

import Application;

export namespace gg
{
	enum class RendererType
	{
		Vulkan,
		D3D12
	};

	struct ApplicationSettings
	{
		uint32_t width;
		uint32_t height;
		SDL_Window* windowHandle;
		RendererType rendererType;
	};

	Application MakeApplication(ApplicationSettings const&);
} // namespace gg 
