module;
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
export module Vertex;

namespace gg
{
	export struct Vertex
	{
		glm::vec4 position{};
		glm::vec4 normal{};
		glm::vec2 textureCoords0{};

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

		Vertex(float x, float y, float z, float w, float u, float v, float nx, float ny, float nz, float nw)
			: position{ x, y, z, w }
			, normal{ nx, ny, nz, nw }
			, textureCoords0{ u, v }
		{
		}
	};

} // namespace gg
