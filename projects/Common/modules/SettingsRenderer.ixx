module;
#include <cstdint>
#include <glm/fwd.hpp>
export module SettingsRenderer;

import Lighting;

export namespace gg
{
	constexpr char const* texturesLocation{ "assets/textures" };
	constexpr char const* texturesExtension{ "tga" };
	constexpr double cubeRotationSpeed{ 0.2 }; // meters per seconds
	constexpr int32_t maxFramesInFlight{ 2 };

	/* Flip the sign of the viewport's height, Y goes up, just in D3D12 */
	constexpr bool flipVulkanViewport{ true };

	DirectionalLight globalDirectionalLight{
		glm::vec3(1.0f, 1.0f, 1.0f), // specular color
		0.5f, // specular strength

		glm::vec3(1.0f, 1.0f, 1.0f), // ambient color
		0.1f, // ambient strength

		glm::vec3(-1.0f, -1.0f, 1.0f), // light direction xyz
		1.0f, // specular shininess // TODO: WTF, this value does not work as expected ?

		glm::vec3(1.0f, 1.0f, 1.0f)   // diffuse color, rgb
	};
} // namespace gg
