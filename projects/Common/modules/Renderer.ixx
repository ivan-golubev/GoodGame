module;
#include <chrono>
#include <cstdint>
#include <expected>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <SDL2/SDL_video.h>
#include <string>
export module Renderer;

import Model;
import TimeManager;
import Input;
import ShaderProgram;
import Texture;
import Camera;
import Scene;
export import ErrorHandling;
import SettingsRenderer;

export namespace gg
{
	struct RendererSettings
	{
		uint32_t    width;
		uint32_t    height;
		SDL_Window* windowHandle;
	};

	class Renderer
	{
	public:
		Renderer()                   = default;
		virtual ~Renderer() noexcept = default;

		Renderer(Renderer const&)            = delete;
		Renderer& operator=(Renderer const&) = delete;

		Renderer(Renderer&&) noexcept            = default;
		Renderer& operator=(Renderer&&) noexcept = default;

		virtual RendererType               GetType() const                     = 0;
		virtual std::expected<void, Error> Initialize(RendererSettings const&) = 0;

		virtual void                       OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual std::expected<void, Error> Render(Scene&)                                   = 0;

		/* Creates the GPU resources for the scene's models and skybox. Returns the errors of entries that could
		   not be loaded; those are skipped when drawing. */
		virtual std::vector<Error> LoadScene(Scene const&) = 0;
	};

	ModelViewProjection CalculateModelViewProjection(glm::mat4x4 const& modelTransform, Camera const&);
	SkyboxCamera        CalculateSkyboxCamera(Camera const&);
} // namespace gg
