module;
#include <SDL2/SDL_video.h>
export module ApplicationSettings;

import Application;

namespace gg
{
	export enum class RendererType
	{
		Vulkan,
		D3D12
	};

	export struct ApplicationSettings
	{
		uint32_t width;
		uint32_t height;
		SDL_Window* windowHandle;
		RendererType rendererType;
	};

	export Application MakeApplication(ApplicationSettings const&);
} // namespace gg 
