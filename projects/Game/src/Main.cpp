#include <cassert>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <format>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <memory>
#include <SDL2/SDL.h>
#include <thread>

import Application;
import ApplicationSettings;
import ErrorHandling;
import ErrorHandlingVulkan;
import Logging;
import ShaderProgram;
import ShaderProgramVulkan;
import SettingsRenderer;
import Texture;

namespace
{
	constexpr uint32_t rendererChangeRequested{ 2 };

	/* Shooter-style mouse look: the cursor is hidden and locked to the window, and SDL reports relative motion.
	   Released (Tab, or when the window loses focus) the cursor is free to use the debug UI. */
	void SetMouseLook(bool enabled)
	{
		SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
		if (enabled)
			SDL_FlushEvent(SDL_MOUSEMOTION); // drop motion queued while the cursor was free, so the view does not jump
	}

	bool IsMouseEvent(SDL_Event const& event)
	{
		return event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL;
	}

	uint32_t MainLoop(std::shared_ptr<gg::Application> app, gg::RendererType& rendererType)
	{
		using namespace gg;

		bool isRunning{ true };
		bool isMouseLook{ true };
		SetMouseLook(isMouseLook);

		while (isRunning)
		{
			SDL_Event event;
			// Poll for user input.
			while (SDL_PollEvent(&event))
			{
				// While looking around, the mouse drives the camera only, not the debug UI.
				if (!(isMouseLook && IsMouseEvent(event)))
					ImGui_ImplSDL2_ProcessEvent(&event);
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
					else if (windowEvent == SDL_WINDOWEVENT_FOCUS_LOST && isMouseLook)
					{
						isMouseLook = false;
						SetMouseLook(isMouseLook);
					}
				}
				break;
				case SDL_MOUSEMOTION:
					if (isMouseLook)
						app->OnMouseMoved(event.motion.xrel, event.motion.yrel);
					break;
				case SDL_KEYDOWN:
					[[fallthrough]];
				case SDL_KEYUP:
				{
					SDL_Keycode key{ event.key.keysym.sym };
					if (key == SDLK_ESCAPE)
						isRunning = false;
					else if (key == SDLK_TAB)
					{
						if (event.type == SDL_KEYDOWN && !event.key.repeat)
						{
							isMouseLook = !isMouseLook;
							SetMouseLook(isMouseLook);
						}
					}
					else if (key == SDLK_F1)
					{
						SetMouseLook(false);
						rendererType = (rendererType == RendererType::Vulkan) ? RendererType::D3D12 : RendererType::Vulkan;
						DebugLog(DebugLevel::Info, std::format("Changing the renderer to {}", ToString(rendererType)));
						return rendererChangeRequested;
					}
					else
						app->OnKeyPressed(key, event.type == SDL_KEYDOWN);
					break;
				}
				default: // NOP
					break;
				}
			}
			// All events processed - tick the world
			if (auto const tickResult{ app->Tick() }; !tickResult)
			{
				DebugLog(DebugLevel::Error, tickResult.error().message);
				return EXIT_FAILURE;
			}
		}
		return EXIT_SUCCESS;
	}

	uint32_t RunApplication(uint32_t width, uint32_t height, gg::RendererType& rendererType)
	{
		using namespace gg;
		auto destroyWindow = [](SDL_Window* window) noexcept
		{
			if (window)
				SDL_DestroyWindow(window);
		};
		std::unique_ptr<SDL_Window, decltype(destroyWindow)> window{
			SDL_CreateWindow("Rendering sample", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE),
			destroyWindow,
		};
		if (!window)
		{
			DebugLog(DebugLevel::Error, "Could not create SDL window");
			return EXIT_FAILURE;
		}

		try
		{
			ApplicationSettings const appSettings{
				.width = width,
				.height = height,
				.windowHandle = window.get(),
				.rendererType = rendererType,
			};
			auto appResult{ MakeApplication(appSettings) };
			if (!appResult)
			{
				DebugLog(DebugLevel::Error, appResult.error().message);
				return EXIT_FAILURE;
			}
			std::shared_ptr<Application> app{ std::move(*appResult) };
			DebugLog(DebugLevel::Info, "Successfully initialized the application");

			std::shared_ptr<Renderer> renderer{ app->GetRenderer() };
			auto handleAssetLoadError = [](Error const& error)
			{
				DebugLog(DebugLevel::Error, error.message);
			};
			if (auto const skyboxResult{ renderer->LoadSkybox("water", "skybox")}; !skyboxResult)
				handleAssetLoadError(skyboxResult.error());
			{
				glm::vec3 position{ -2.f, 0.f, 0.f };
				if (auto const result{ renderer->LoadModel("assets/models/textured_cube.glb", "textured_surface", position) }; !result)
					handleAssetLoadError(result.error());
			}
			{
				glm::vec3 position{ 2.f, 0.f, 0.f };
				if (auto const result{ renderer->LoadModel("assets/models/textured_cone.glb", "textured_surface", position) }; !result)
					handleAssetLoadError(result.error());
			}

			uint32_t status = MainLoop(app, rendererType);
			Application::Destroy();

			if (status != EXIT_SUCCESS)
				return status;
		}
		catch (std::exception const& e)
		{
			DebugLog(DebugLevel::Error, e.what());
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
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
	RendererType rendererType{ RendererType::Vulkan };
	uint32_t status{ 0 };
	while (status = RunApplication(width, height, rendererType) == rendererChangeRequested) {}
	return status;
}
