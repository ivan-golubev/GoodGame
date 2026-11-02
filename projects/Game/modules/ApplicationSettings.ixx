module;
#include <memory>
#include <expected>
#include <SDL2/SDL_video.h>
#include <string>
export module ApplicationSettings;

import Application;
import ErrorHandling;
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

	std::expected<std::shared_ptr<Application>, Error> MakeApplication(ApplicationSettings const&);
} // namespace gg 
