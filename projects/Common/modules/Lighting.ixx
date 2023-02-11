module;
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
export module Lighting;


export namespace gg
{
	struct DirectionalLight
	{
		glm::vec3 diffuseColor;
		glm::vec3 lightDirection;

		constexpr DirectionalLight(float r, float g, float b, float x, float y, float z)
			: diffuseColor{ r, g, b }
			, lightDirection{ x, y, z }
		{
		}
	};

} // namespace gg
