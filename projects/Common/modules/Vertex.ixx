module;
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
export module Vertex;

namespace gg
{
	export struct Vertex
	{
		glm::vec4 position{};
		glm::vec2 textureCoords0{};
		glm::vec3 normals{};

		Vertex() = default;

		Vertex(float x, float y, float z, float w)
			: position{ x, y, z, w }
		{
		}

		Vertex(float x, float y, float z, float w, float u, float v)
			: position{ x, y, z, w }
			, textureCoords0{ u, v }
		{
		}

		Vertex(float x, float y, float z, float w, float u, float v, float nx, float ny, float nz)
			: position{ x, y, z, w }
			, textureCoords0{ u, v }
			, normals{ nx, ny, nz }
		{
		}
	};

} // namespace gg
