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
	export class Application 
	{
	public:
		Application(std::unique_ptr<Renderer>, std::shared_ptr<TimeManager>, std::shared_ptr<InputManager>);
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
		bool isPaused{ false };

		std::shared_ptr<InputManager> inputManager;
		std::shared_ptr<ModelLoader> modelLoader{ std::make_shared<ModelLoader>() };
		std::shared_ptr<TimeManager> timeManager;
		std::shared_ptr<Renderer> renderer;
	};
} // namespace gg 
