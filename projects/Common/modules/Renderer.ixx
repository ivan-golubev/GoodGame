module;
#include <cstdint>
#include <chrono>
#include <memory>
#include <DirectXMath.h>
#include <SDL2/SDL_video.h>
export module Renderer;

import Model;
import TimeManager;
import Input;
import ShaderProgram;
import Texture;
import Camera;
import Lighting;

using std::chrono::nanoseconds;
using DirectX::XMVECTOR;
using DirectX::XMMATRIX;

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

		virtual void OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual void Render(nanoseconds deltaTime) = 0;
		virtual std::shared_ptr<ShaderProgram> LoadShader(std::string const& shaderName) = 0;
		virtual void LoadModel(std::string const& modelRelativePath, std::string const& shaderName, XMVECTOR& position) = 0;
	};

	struct RendererSettings
	{
		uint32_t width;
		uint32_t height;
		SDL_Window* windowHandle;
		std::shared_ptr<TimeManager> timeManager;
		std::shared_ptr<InputManager> inputManager;
	};

	XMMATRIX CalculateMVP(XMMATRIX modelTranslation, double currentTimeSec, Camera const&);
	XMMATRIX CalculateNormalMatrix(XMMATRIX modelTranslation);

	constexpr char const* texturesLocation{ "assets/textures" };
	constexpr char const* texturesExtension{ "tga" };
	constexpr double cubeRotationSpeed{ 0.2 }; // meters per seconds
	constexpr int32_t maxFramesInFlight{ 2 };

	/* Flip the sign of the viewport's height, Y goes up, just in D3D12 */
	constexpr bool flipVulkanViewport{ true };

	constexpr DirectionalLight globalDirectionalLight{
		1.0f, 1.0f, 1.0f,   // rgb
		-1.0f, 0.0f, 0.0f // xyz
	};

} // namespace gg
