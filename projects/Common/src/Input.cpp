module;
#include <chrono>
#include <cstdint>
#include <cstring> // memset
#include <SDL2/SDL_keycode.h>
module Input;

using std::chrono::milliseconds;

namespace gg
{
	void InputManager::OnKeyPressed(SDL_Keycode key, bool isDown)
	{
		if (key == SDLK_w)
			keys[MoveCameraForward] = isDown;
		else if (key == SDLK_s)
			keys[MoveCameraBack] = isDown;
		else if (key == SDLK_a)
			keys[MoveCameraLeft] = isDown;
		else if (key == SDLK_d)
			keys[MoveCameraRight] = isDown;
		else if (key == SDLK_q)
			keys[RaiseCamera] = isDown;
		else if (key == SDLK_e)
			keys[LowerCamera] = isDown;
		else if (key == SDLK_UP)
			keys[LookCameraUp] = isDown;
		else if (key == SDLK_DOWN)
			keys[LookCameraDown] = isDown;
		else if (key == SDLK_LEFT)
			keys[TurnCameraLeft] = isDown;
		else if (key == SDLK_RIGHT)
			keys[TurnCameraRight] = isDown;
	}

	void InputManager::SetKeyDown(InputAction a, bool value)
	{
		keys[a] = value;
	}

	void InputManager::ClearKeys()
	{
		memset(keys, 0, sizeof(keys));
	}

	bool InputManager::IsKeyDown(InputAction a) const
	{
		return keys[a];
	}

	float InputManager::GetPlayerSpeed(milliseconds delta) const
	{
		return static_cast<float>(playerSpeed / 1000 * delta.count());
	}

} // namespace gg
