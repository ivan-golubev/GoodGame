module;
#include <cstdint>
#include <expected>
#include <memory>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <windows.h>
export module Application;

import Renderer;
import Input;
import TimeManager;

namespace gg
{
	export class Application
	{
	public:
		static std::shared_ptr<Application> Init(std::unique_ptr<Renderer>, std::shared_ptr<TimeManager>, std::shared_ptr<InputManager>);
		static void                         Destroy();
		static bool                         IsInitialized();
		static std::shared_ptr<Application> Get();

		/* Only Application can make a PassKey, so only Init() can create an Application,
		   while std::make_shared can still call the public constructor. */
		class PassKey
		{
			friend class Application;
			PassKey() = default;
		};
		Application(PassKey, std::unique_ptr<Renderer>, std::shared_ptr<TimeManager>, std::shared_ptr<InputManager>);

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

		std::shared_ptr<InputManager> GetInputManager();
		std::shared_ptr<TimeManager>  GetTimeManager();
		std::shared_ptr<Renderer>     GetRenderer();

	private:
		static std::shared_ptr<Application> instance;

		bool isPaused{ false };

		std::shared_ptr<InputManager> inputManager;
		std::shared_ptr<TimeManager>  timeManager;
		std::shared_ptr<Renderer>     renderer;
	};
} // namespace gg
