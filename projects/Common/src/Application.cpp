module;
#include <cstdint>
#include <DirectXMath.h>
#include <memory>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
module Application;

import Renderer;
import Input;
import Logging;
import TimeManager;
import ErrorHandling;

namespace gg
{
	Application::Application(std::unique_ptr<Renderer> renderer, std::shared_ptr<TimeManager> timeManager, std::shared_ptr<InputManager> inputManager)
		: renderer{ std::move(renderer) }
		, inputManager{ inputManager }
		, timeManager{ timeManager }
	{
		/* Check for DirectX Math library support. */
		if (!DirectX::XMVerifyCPUSupport())
			throw ApplicationInitException("Failed to verify DirectX Math library support");
	}

	Application::~Application() 
	{
		DebugLog(DebugLevel::Info, "Shutting down the application");
	}

	std::shared_ptr<TimeManager> Application::GetTimeManager() 
	{
		return timeManager;
	}

	std::shared_ptr<InputManager> Application::GetInputManager()
	{
		return inputManager;
	}

	std::shared_ptr<ModelLoader> Application::GetModelLoader()
	{
		return modelLoader;
	}

	std::shared_ptr<Renderer> Application::GetRenderer()
	{
		return renderer;
	}

	void Application::Tick()
	{
		if (!isPaused)
			renderer->Render(timeManager->Tick());
	}

	void Application::OnWindowResized(uint32_t width, uint32_t height)
	{
		renderer->OnWindowResized(width, height);
	}

	void Application::OnWindowMinimized()
	{
		isPaused = true;
	}

	void Application::OnWindowRestored()
	{
		isPaused = false;
	}

	void Application::OnKeyPressed(SDL_Keycode key, bool isDown)
	{
		inputManager->OnKeyPressed(key, isDown);
	}

} // namespace gg 
