module;
#include <chrono>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <SDL2/SDL_video.h>
export module Renderer;

import Model;
import TimeManager;
import Input;
import ShaderProgram;
import Texture;
import Camera;

using std::chrono::nanoseconds;

export namespace gg
{
	class Renderer
	{
	public:
		Renderer() = default;
		virtual ~Renderer() = default;

		Renderer(Renderer const&) = delete;
		Renderer& operator=(Renderer const&) = delete;

		Renderer(Renderer&&) noexcept = default;
		Renderer& operator=(Renderer&&) noexcept = default;

		virtual std::string Name() const = 0;
		virtual std::shared_ptr<Camera> GetCamera() = 0;

		virtual void OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual void Render(nanoseconds deltaTime) = 0;
		virtual std::shared_ptr<ShaderProgram> LoadShader(std::string const& shaderName) = 0;
		virtual void LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position) = 0;
	};

	struct RendererSettings
	{
		uint32_t width;
		uint32_t height;
		SDL_Window* windowHandle;
		std::shared_ptr<TimeManager> timeManager;
		std::shared_ptr<InputManager> inputManager;
	};

	glm::mat4x4 CalculateMVP(glm::mat4x4 modelTranslation, double currentTimeSec, Camera const&);
} // namespace gg
