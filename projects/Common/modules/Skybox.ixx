module;
#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
export module Skybox;

export import ErrorHandling;
import ShaderProgram;
import Texture;

export namespace gg
{
	/* API-independent part of a skybox: the six cube map face images and the shader used to draw them. */
	class Skybox
	{
	public:
		Skybox(std::string const& name, std::shared_ptr<ShaderProgram>);
		virtual ~Skybox() noexcept       = default;
		Skybox(Skybox const&)            = delete;
		Skybox& operator=(Skybox const&) = delete;

		/* Loads the face images and checks they can form a cube map. */
		std::expected<void, Error> Initialize();

		static constexpr size_t faceCount{ 6 };

	protected:
		std::shared_ptr<ShaderProgram> shaderProgram;
		/* In cube map layer order: +X, -X, +Y, -Y, +Z, -Z (right, left, top, bottom, front, back). */
		std::array<std::unique_ptr<Texture>, faceCount> textures{};
	};
} // namespace gg
