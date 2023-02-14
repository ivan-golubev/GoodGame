module;
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <vector>
export module Model;

import Vertex;
import ShaderProgram;
import Texture;


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
	};

	struct Model
	{
		Model(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, glm::vec3& position);
		virtual ~Model() noexcept = default;
		Model(Model const&) = delete;
		Model& operator=(Model const&) = delete;
		Model(Model&&) noexcept;
		Model& operator=(Model&&) noexcept;

		glm::mat4x4 translation{};

		void SetPosition(glm::vec3& position);

		std::shared_ptr<ShaderProgram> shaderProgram;
		std::vector<std::string> textureNames;
		std::vector<std::shared_ptr<Texture>> textures;
		std::vector<Mesh> meshes;
		std::string name;
	};

} // namespace gg
