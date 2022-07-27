module;
#include <cstdint>
#include <memory>
#include <SDL2/SDL_video.h>
export module Renderer;

import Model;

namespace gg
{
	export class Renderer
	{
	public:
		Renderer(uint32_t width, uint32_t height, SDL_Window*);
		virtual ~Renderer() = default;
		virtual void UploadGeometry(std::unique_ptr<Model>) = 0;
		virtual void OnWindowResized(uint32_t width, uint32_t height) = 0;
		virtual void Render(uint64_t deltaTimeMs) = 0;
	protected:
		uint32_t mWidth{};
		uint32_t mHeight{};
		SDL_Window* mWindowHandle{};
	};

} // namespace gg
