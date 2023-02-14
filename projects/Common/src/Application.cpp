module;
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
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
	std::shared_ptr<Application> Application::INSTANCE{ nullptr };

	std::shared_ptr<Application> Application::Init(std::unique_ptr<Renderer> renderer, std::shared_ptr<TimeManager> timeManager, std::shared_ptr<InputManager> inputManager)
	{
		BreakIfFalse(!Application::IsInitialized());
		Application* app = new Application(std::move(renderer), timeManager, inputManager);
		INSTANCE = std::shared_ptr<Application>(app);
		return INSTANCE;
	}

	void Application::Destroy()
	{
		BreakIfFalse(Application::IsInitialized());
		INSTANCE.reset();
	}

	bool Application::IsInitialized()
	{
		return INSTANCE.get() != nullptr;
	}

	std::shared_ptr<Application> Application::Get()
	{
		BreakIfFalse(Application::IsInitialized());
		return INSTANCE;
	}

	Application::Application(std::unique_ptr<Renderer> renderer, std::shared_ptr<TimeManager> timeManager, std::shared_ptr<InputManager> inputManager)
		: renderer{ std::move(renderer) }
		, inputManager{ inputManager }
		, timeManager{ timeManager }
	{
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
