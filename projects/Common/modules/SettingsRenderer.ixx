module;
#include <cstdint>
#include <glm/fwd.hpp>
#include <string>
#include <string_view>
export module SettingsRenderer;

import Lighting;

namespace
{
	constexpr std::string_view rendererName[2]{
		"Vulkan",
		"D3D12"
	};
} // namespace

export namespace gg
{
	constexpr std::string_view texturesLocation{ "assets/textures" };
	constexpr std::string_view texturesExtension{ "tga" };
	constexpr double           cubeRotationSpeed{ 0.2 }; // meters per seconds
	constexpr int32_t          maxFramesInFlight{ 2 };

	/* Flip the sign of the viewport's height, Y goes up, just in D3D12 */
	constexpr bool flipVulkanViewport{ true };

	/* Starting values for Scene::light */
	inline DirectionalLight const defaultDirectionalLight{
		glm::vec3(1.f, 1.f, 1.f), // specular color
		0.5f, // specular strength

		glm::vec3(1.f, 1.f, 1.f), // ambient color
		0.1f, // ambient strength

		glm::vec3(-1.f, -1.f, 1.f), // light direction xyz
		1.f, // specular shininess // TODO: WTF, this value does not work as expected ?

		glm::vec3(1.f, 1.f, 1.f) // diffuse color, rgb
	};

	enum RendererType
	{
		Vulkan,
		D3D12,
	};

	std::string ToString(RendererType r)
	{
		return std::string{ rendererName[r] };
	}
} // namespace gg
