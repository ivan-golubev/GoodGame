module;
#include <chrono>
#include <cstdint>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <numbers>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <tuple>
module Application;

import Renderer;
import Input;
import Logging;
import TimeManager;
import ErrorHandling;
import SettingsRenderer;

using namespace std::chrono_literals;
using std::chrono::nanoseconds;

namespace gg
{
	Application::Application()
		: inputManager{ std::make_shared<InputManager>() }
		, timeManager{ std::make_shared<TimeManager>() }
		, scene{ inputManager }
	{
	}

	Application::~Application() noexcept
	{
		DebugLog(DebugLevel::Info, "Shutting down the application");
	}

	Scene& Application::GetScene()
	{
		return scene;
	}

	std::vector<Error> Application::SetRenderer(std::unique_ptr<Renderer> newRenderer)
	{
		renderer = std::move(newRenderer);
		if (!renderer)
			return {};
		/* Restart frame timing, so the time spent creating the renderer is not one long frame */
		std::ignore = timeManager->Tick();
		return renderer->LoadScene(scene);
	}

	std::expected<void, Error> Application::Tick()
	{
		if (isPaused || !renderer)
			return {};

		/* Game logic first, then draw the result */
		nanoseconds const deltaTime{ timeManager->Tick() };
		scene.GetCamera().UpdateCamera(deltaTime);

		float const spinAngle{ static_cast<float>(cubeRotationSpeed * std::numbers::pi_v<double> * (deltaTime / 1.0s)) };
		for (SceneModel& model : scene.GetModels())
			model.transform = glm::rotate(model.transform, spinAngle, glm::vec3{ 1.f, 0.f, 1.f });

		return renderer->Render(scene);
	}

	void Application::OnWindowResized(uint32_t width, uint32_t height)
	{
		if (renderer)
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
