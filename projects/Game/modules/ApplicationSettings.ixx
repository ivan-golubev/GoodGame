module;
#include <expected>
#include <memory>
#include <SDL2/SDL_video.h>
#include <string>
export module ApplicationSettings;

import Renderer;
import ErrorHandling;
import SettingsRenderer;

export namespace gg
{
	struct ApplicationSettings
	{
		uint32_t     width;
		uint32_t     height;
		SDL_Window*  windowHandle;
		RendererType rendererType;
	};

	/* Creates and initializes the renderer the settings ask for, rendering into the settings' window */
	std::expected<std::unique_ptr<Renderer>, Error> MakeRenderer(ApplicationSettings const&);
} // namespace gg
