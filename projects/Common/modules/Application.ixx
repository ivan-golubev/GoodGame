module;
#include <cstdint>
#include <expected>
#include <memory>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <vector>
#include <windows.h>
export module Application;

import Renderer;
import Input;
import TimeManager;
import Scene;

namespace gg
{
	export class Application
	{
	public:
		/* Creates the input and time managers and the scene. A renderer is attached with SetRenderer. */
		Application();

		~Application() noexcept;

		Application(Application const&)            = delete;
		Application& operator=(Application const&) = delete;

		Application(Application&&) noexcept            = default;
		Application& operator=(Application&&) noexcept = default;

		std::expected<void, Error> Tick();
		void                       OnWindowResized(uint32_t width, uint32_t height);
		void                       OnWindowMinimized();
		void                       OnWindowRestored();
		void                       OnKeyPressed(SDL_Keycode, bool isDown);
		void                       OnMouseMoved(int32_t deltaX, int32_t deltaY);

		Scene& GetScene();

		/* Attaches the renderer that draws the scene from now on and creates its GPU resources for the scene's
		   contents. Returns the errors of entries that could not be loaded; they are skipped when drawing.
		   nullptr releases the current renderer, which must happen before its window is destroyed. */
		std::vector<Error> SetRenderer(std::unique_ptr<Renderer>);

	private:
		bool isPaused{ false };

		std::shared_ptr<InputManager> inputManager;
		std::shared_ptr<TimeManager>  timeManager;
		std::unique_ptr<Renderer>     renderer;

		Scene scene; /* outlives renderer switches, so the camera, light and models are kept */
	};
} // namespace gg
