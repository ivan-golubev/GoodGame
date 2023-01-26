#include <SDL2/SDL.h>
#include <exception>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cassert>
#include <memory>

import Application;
import ApplicationSettings;
import ErrorHandling;
import ErrorHandlingVulkan;
import Logging;
import ModelLoader;
import ShaderProgram;

namespace
{
	void MainLoop(gg::Application& app)
	{
		using namespace std::chrono_literals;
		using namespace gg;

		bool isRunning{ true };

		std::shared_ptr<TimeManager> timeManager{ app.GetTimeManager() };
		int64_t lastEventPollMs{ 0 };
		constexpr int64_t EVENT_POLL_INTERVAL_MS{ 1000 / 60 }; // Poll 60 times per sec.

		while (isRunning)
		{
			int64_t currentTimeMs{ timeManager->GetCurrentTimeMs().count() };
			bool needToPollEvents{ currentTimeMs - lastEventPollMs > EVENT_POLL_INTERVAL_MS };

			if (needToPollEvents)
			{
				lastEventPollMs = currentTimeMs;

				SDL_Event event;
				// Poll for user input.
				while (SDL_PollEvent(&event))
				{
					switch (event.type)
					{
					case SDL_QUIT:
						isRunning = false;
						break;
					case SDL_WINDOWEVENT:
					{
						uint8_t windowEvent{ event.window.event };
						if (windowEvent == SDL_WINDOWEVENT_RESIZED)
							app.OnWindowResized(event.window.data1, event.window.data2);
						else if (windowEvent == SDL_WINDOWEVENT_MINIMIZED)
							app.OnWindowMinimized();
						else if (windowEvent == SDL_WINDOWEVENT_RESTORED)
							app.OnWindowRestored();
					}
					break;
					case SDL_KEYDOWN:
						[[fallthrough]];
					case SDL_KEYUP:
					{
						SDL_Keycode key{ event.key.keysym.sym };
						if (key == SDLK_ESCAPE)
							isRunning = false;
						else
							app.OnKeyPressed(key, event.type == SDL_KEYDOWN);
						break;
					}
					default: // NOP
						break;
					}
				}
			}
			app.Tick();
			/* Don't do the next tick immediately */
			// TODO: why we need this ? maybe just the input polling has to be slower ? 250ms is a lot
			std::this_thread::sleep_for(0.25ms);
		}
	}
} // namespace

int main()
{
	using namespace gg;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		DebugLog(DebugLevel::Error, "Could not initialize SDL");
		return EXIT_FAILURE;
	}
	atexit(SDL_Quit);

	constexpr uint32_t width{ 1920 };
	constexpr uint32_t height{ 1080 };

	SDL_Window* window = SDL_CreateWindow("Vulkan Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window)
	{
		DebugLog(DebugLevel::Error, "Could not create SDL window");
		return EXIT_FAILURE;
	}

	try
	{
		ApplicationSettings const appSettings{ width, height, window, RendererType::Vulkan };
		Application app{ MakeApplication(appSettings) };
		DebugLog(DebugLevel::Info, "Successfully initialized the Vulkan application");

		std::unique_ptr<ShaderProgram> shader{ std::make_unique<ShaderProgram>("shaders/textured_surface_VS.spv", "shaders/textured_surface_PS.spv") };
		std::unique_ptr<Model> model{ app.GetModelLoader()->LoadModel("../../../assets/runtime/models/textured_cube.glb", std::move(shader)) };

		app.GetRenderer()->UploadGeometry(std::move(model));
		MainLoop(app);
	}
	catch (std::exception const& e)
	{
		DebugLog(DebugLevel::Error, e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
