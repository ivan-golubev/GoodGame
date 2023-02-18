module;
#include <memory>
#include <SDL2/SDL_video.h>
#include <string>
export module ApplicationSettings;

import Application;
import SettingsRenderer;

export namespace gg
{
	struct ApplicationSettings
	{
		uint32_t width;
		uint32_t height;
		SDL_Window* windowHandle;
		RendererType rendererType;
	};

	std::shared_ptr<Application> MakeApplication(ApplicationSettings const&);
} // namespace gg 
