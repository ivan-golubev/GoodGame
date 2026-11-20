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

using std::chrono::nanoseconds;

export namespace gg
{
	struct RendererSettings
	{
		uint32_t                     width;
		uint32_t                     height;
		SDL_Window*                  windowHandle;
		std::shared_ptr<TimeManager> timeManager;
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

		virtual void                                                 OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual std::expected<void, Error>                           Render(nanoseconds deltaTime, Scene&)            = 0;
		virtual std::expected<std::shared_ptr<ShaderProgram>, Error> LoadShader(std::string const& shaderName)        = 0;

		virtual std::expected<void, Error> LoadModel(
			std::string const& modelRelativePath,
			std::string const& shaderName,
			glm::vec3&         position
		) = 0;

		virtual std::expected<void, Error> LoadSkybox(std::string const& name, std::string const& shaderName) = 0;
	};

	ModelViewProjection CalculateModelViewProjection(glm::mat4x4 modelTranslation, double currentTimeSec, Camera const&);
	SkyboxCamera        CalculateSkyboxCamera(Camera const&);
} // namespace gg
