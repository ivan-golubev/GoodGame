module;
#include <cstdint>
#include <chrono>
#include <memory>
#include <SDL2/SDL_video.h>
export module Renderer;

import Model;
import TimeManager;
import Input;
import ShaderProgram;
import Texture;

using std::chrono::nanoseconds;

namespace gg
{
	export class Renderer
	{
	public:
		Renderer() = default;
		virtual ~Renderer() = default;

		Renderer(Renderer const&) = delete;
		Renderer& operator=(Renderer const&) = delete;

		Renderer(Renderer&&) noexcept = default;
		Renderer& operator=(Renderer&&) noexcept = default;

		virtual void OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual void Render(nanoseconds deltaTime) = 0;
		virtual std::unique_ptr<ShaderProgram> LoadShader(std::string const& shaderName) = 0;
		virtual std::shared_ptr<Texture> LoadTexture(std::string const& textureRelativePath) = 0;
		virtual void LoadModel(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram>, std::shared_ptr<Texture>) = 0;
	};

	export struct RendererSettings
	{
		uint32_t width;
		uint32_t height;
		SDL_Window* windowHandle;
		std::shared_ptr<TimeManager> timeManager;
		std::shared_ptr<InputManager> inputManager;
	};

} // namespace gg
