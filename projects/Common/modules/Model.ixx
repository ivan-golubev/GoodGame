module;
#include <memory>
#include <vector>
export module Model;

import Vertex;
import ShaderProgram;

namespace gg
{
	export struct Mesh
	{
		Mesh() = default;
		~Mesh() noexcept = default;
		Mesh(Mesh const&) = delete;
		Mesh& operator=(Mesh const&) = delete;
		Mesh(Mesh&&) noexcept;
		Mesh& operator=(Mesh&&) noexcept;

		std::vector<Vertex> Vertices{};
		std::vector<uint32_t> Indices{};

		uint8_t* Texture{ nullptr };
		uint32_t TextureColorFormat{ 0 }; /* GL_RGB or GL_RGBA */
		uint32_t TextureWidth{ 0 };
		uint32_t TextureHeight{ 0 };

		uint32_t VerticesSizeBytes() const;
		uint32_t IndicesSizeBytes() const;
		uint32_t GetVertexCount() const;
		uint32_t GetIndexCount() const;
	};

	export struct Model
	{
	public:
		Model() = default;
		~Model() noexcept = default;
		Model(Model const&) = delete;
		Model& operator=(Model const&) = delete;
		Model(Model&&) noexcept;
		Model& operator=(Model&&) noexcept;

		std::shared_ptr<ShaderProgram> shaderProgram;
		std::vector<Mesh> meshes;
	};

} // namespace gg
