module;
#include <cstdint>
#include <chrono>
#include <memory>
#include <SDL2/SDL_video.h>
export module Renderer;

import Model;

namespace gg
{
	export class Renderer
	{
	public:
		Renderer() = default;
		virtual ~Renderer() = default;

		Renderer(Renderer const&) = delete;
		Renderer& operator=(Renderer const&) = delete;

		Renderer(Renderer &&) noexcept = default;
		Renderer& operator=(Renderer &&) noexcept = default;

		virtual void UploadGeometry(std::unique_ptr<Model>) = 0;
		virtual void OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual void Render(std::chrono::milliseconds deltaTime) = 0;
	};

} // namespace gg
