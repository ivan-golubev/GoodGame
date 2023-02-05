module;
#include <string>
#include <memory>
#include <vector>
#include <DirectXMath.h>
export module Model;

import Vertex;
import ShaderProgram;
import Texture;

using DirectX::XMVECTOR;
using DirectX::XMMATRIX;

export namespace gg
{
	struct Mesh
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

	struct Model
	{
		Model(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, std::shared_ptr<Texture>);
		virtual ~Model() noexcept = default;
		Model(Model const&) = delete;
		Model& operator=(Model const&) = delete;
		Model(Model&&) noexcept;
		Model& operator=(Model&&) noexcept;

		XMMATRIX translation{};

		void SetPosition(XMVECTOR& position);

		std::shared_ptr<ShaderProgram> shaderProgram;
		std::shared_ptr<Texture> texture;
		std::vector<Mesh> meshes;
	};

} // namespace gg
