module;
#include <chrono>
#include <cstdint>
#include <SDL2/SDL_keycode.h>
export module Input;

using std::chrono::milliseconds;

export namespace gg
{
	enum InputAction
	{
		RaiseCamera,
		LowerCamera,
		MoveCameraLeft,
		MoveCameraRight,
		MoveCameraForward,
		MoveCameraBack,
		TurnCameraLeft,
		TurnCameraRight,
		LookCameraUp,
		LookCameraDown,
		Count
	};

	struct MouseDelta
	{
		float x{}; // pixels, positive = right
		float y{}; // pixels, positive = down
	};

	class InputManager
	{
	public:
		void OnKeyPressed(SDL_Keycode, bool isDown);
		void OnMouseMoved(int32_t deltaX, int32_t deltaY);
		MouseDelta ConsumeMouseDelta(); /* returns the motion accumulated since the last call and resets it */
		void SetKeyDown(InputAction a, bool value);
		void ClearKeys();
		bool IsKeyDown(InputAction a) const;
		float GetPlayerSpeed(milliseconds delta) const;
	private:
		bool keys[InputAction::Count]{};
		MouseDelta mouseDelta{};
		static constexpr float playerSpeed{ 1.5f };
	};

} // namespace gg
