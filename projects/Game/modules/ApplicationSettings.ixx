module;
#include <SDL2/SDL_video.h>
#include <memory>
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

	std::shared_ptr<Application> MakeApplication(ApplicationSettings const&);
} // namespace gg 
