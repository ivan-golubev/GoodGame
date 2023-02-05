#include <SDL2/SDL.h>
#include <exception>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cassert>
#include <memory>
#include <DirectXMath.h>

import Application;
import ApplicationSettings;
import ErrorHandling;
import ErrorHandlingVulkan;
import Logging;
import ModelLoader;
import ShaderProgram;
import ShaderProgramVulkan;
import Texture;

using DirectX::XMVECTOR;

namespace
{
	void MainLoop(std::shared_ptr<gg::Application> app)
	{
		using namespace gg;

		bool isRunning{ true };

		while (isRunning)
		{
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
						app->OnWindowResized(event.window.data1, event.window.data2);
					else if (windowEvent == SDL_WINDOWEVENT_MINIMIZED)
						app->OnWindowMinimized();
					else if (windowEvent == SDL_WINDOWEVENT_RESTORED)
						app->OnWindowRestored();
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
						app->OnKeyPressed(key, event.type == SDL_KEYDOWN);
					break;
				}
				default: // NOP
					break;
				}
			}
			// All events processed - tick the world
			app->Tick();
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

	SDL_Window* window = SDL_CreateWindow("Rendering sample", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window)
	{
		DebugLog(DebugLevel::Error, "Could not create SDL window");
		return EXIT_FAILURE;
	}

	try
	{
		ApplicationSettings const appSettings{ width, height, window, RendererType::D3D12 };
		std::shared_ptr<Application> app{ MakeApplication(appSettings) };
		DebugLog(DebugLevel::Info, "Successfully initialized the application");

		std::shared_ptr<Renderer> renderer{ app->GetRenderer() };
		{
			XMVECTOR position{ 0.f, 0.f, 0.f };
			renderer->LoadModel("assets/models/textured_cube.glb", "textured_surface", position);
		}
		// TODO: need to handle multiple models, each with it's own shader, textures and positions.
		// should be multiple draw calls. Guess everything is hard-coded right now.
		{
			//XMVECTOR position{ 1.f, 1.f, 1.f };
			//renderer->LoadModel("assets/models/textured_cube.glb", "textured_surface", position);
		}

		MainLoop(app);
		Application::Destroy();
	}
	catch (std::exception const& e)
	{
		DebugLog(DebugLevel::Error, e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
