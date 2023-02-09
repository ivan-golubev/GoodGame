module;
#include <cstdint>
export module GlobalSettings;

import Lighting;

export namespace gg
{
	consteval bool IsDebug()
	{
#ifdef _DEBUG
		return true;
#else
		return false;
#endif
	}

	consteval bool IsFinal()
	{
#ifdef FINAL
		return true;
#else
		return false;
#endif
	}

	consteval bool IsWindowsSubSystem()
	{
#ifdef _CONSOLE
		return false;
#else
		return true;
#endif
	}

	constexpr char const* texturesLocation{ "assets/textures" };
	constexpr char const* texturesExtension{ "tga" };
	constexpr double cubeRotationSpeed{ 0.2 }; // meters per seconds
	constexpr int32_t maxFramesInFlight{ 2 };

	constexpr DirectionalLight globalDirectionalLight{
		0.3f, 0.3f, 0.3f, 1.0f,  // rgba
		0.0f, 0.0f, -3.0f // xyz
	};

} // namespace gg
