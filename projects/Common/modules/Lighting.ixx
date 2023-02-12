module;
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
export module Lighting;


export namespace gg
{
	struct DirectionalLight
	{
		glm::vec3 specularColor;
		float specularStrength;

		glm::vec3 ambientColor;
		float ambientStrength;

		glm::vec3 lightDirection;
		float specularShininess;

		glm::vec3 diffuseColor;
	};

} // namespace gg
