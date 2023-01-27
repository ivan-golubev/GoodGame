module;
#include <memory>
#include <vector>
export module Model;

import Vertex;
import ShaderProgram;
import Texture;

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

		uint64_t VerticesSizeBytes() const;
		uint64_t IndicesSizeBytes() const;
		uint32_t GetVertexCount() const;
		uint32_t GetIndexCount() const;

		std::vector<Vertex> vertices{};
		std::vector<uint32_t> indices{};

		uint32_t textureColorFormat{ 0 }; /* GL_RGB or GL_RGBA */
		uint32_t textureWidth{ 0 };
		uint32_t textureHeight{ 0 };
		uint8_t* texture{ nullptr };
	};

	export struct Model
	{
		Model(std::unique_ptr<ShaderProgram>, std::unique_ptr<Texture>);
		~Model() noexcept = default;
		Model(Model const&) = delete;
		Model& operator=(Model const&) = delete;
		Model(Model&&) noexcept;
		Model& operator=(Model&&) noexcept;

		std::unique_ptr<ShaderProgram> shaderProgram;
		std::unique_ptr<Texture> texture;
		std::vector<Mesh> meshes;
	};

} // namespace gg
