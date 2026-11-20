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
	Application::Application(
		std::unique_ptr<Renderer>     renderer,
		std::shared_ptr<TimeManager>  timeManager,
		std::shared_ptr<InputManager> inputManager
	)
		: renderer{ std::move(renderer) }
		, inputManager{ inputManager }
		, timeManager{ timeManager }
		, scene{ this->inputManager }
	{
	}

	Application::~Application() noexcept
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

	std::expected<void, Error> Application::Tick()
	{
		if (!isPaused)
			return renderer->Render(timeManager->Tick(), scene);
		return {};
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

	void Application::OnMouseMoved(int32_t deltaX, int32_t deltaY)
	{
		inputManager->OnMouseMoved(deltaX, deltaY);
	}

} // namespace gg
