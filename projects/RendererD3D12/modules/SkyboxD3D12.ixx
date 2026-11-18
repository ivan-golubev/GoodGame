module;
#include <expected>
#include <memory>
#include <string>
export module SkyboxD3D12;

import ErrorHandling;
import ShaderProgram;
import Skybox;

export namespace gg
{
	class SkyboxD3D12 : public Skybox
	{
	public:
		SkyboxD3D12(std::string const& name, std::shared_ptr<ShaderProgram>);
		~SkyboxD3D12() noexcept override = default;

		std::expected<void, Error> Initialize();
	};
} // namespace gg
