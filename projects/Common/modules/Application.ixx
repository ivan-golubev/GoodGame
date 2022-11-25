module;
#include <cstdint>
#include <memory>
#include <windows.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_keycode.h>
export module Application;

import Renderer;
import Input;
import TimeManager;
import ModelLoader;

namespace gg 
{
	struct ApplicationSettings;

	export class Application 
	{
	public:
		static std::shared_ptr<Application> Init(std::unique_ptr<Renderer>);
		static void Destroy();
		static bool IsInitialized();
		static std::shared_ptr<Application> Get();

		Application(std::unique_ptr<Renderer>);
		~Application();
		void Tick();
		void OnWindowResized(uint32_t width, uint32_t height);
		void OnWindowMinimized();
		void OnWindowRestored();
		void OnKeyPressed(SDL_Keycode, bool isDown);

		std::shared_ptr<InputManager> GetInputManager();
		std::shared_ptr<ModelLoader> GetModelLoader();
		std::shared_ptr<TimeManager> GetTimeManager();
		std::shared_ptr<Renderer> GetRenderer();
	private:
		// TODO: remove the singleton !
		static std::shared_ptr<Application> INSTANCE;

		bool isPaused{ false };

		std::shared_ptr<InputManager> inputManager{ std::make_unique<InputManager>() };
		std::shared_ptr<ModelLoader> modelLoader{ std::make_unique<ModelLoader>() };
		std::shared_ptr<TimeManager> timeManager{ std::make_unique<TimeManager>() };
		std::shared_ptr<Renderer> renderer;

		friend std::shared_ptr<Application> MakeApplication(ApplicationSettings);
	};
} // namespace gg 
