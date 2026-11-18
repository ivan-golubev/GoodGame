module;
#include <expected>
#include <memory>
#include <string>
module SkyboxD3D12;

import ErrorHandling;
import ShaderProgram;
import Skybox;

namespace gg
{
	SkyboxD3D12::SkyboxD3D12(std::string const& name, std::shared_ptr<ShaderProgram> s)
		: Skybox{ name, s }
	{
	}

	std::expected<void, Error> SkyboxD3D12::Initialize()
	{
		if (auto result{ Skybox::Initialize() }; !result)
			return std::unexpected(result.error());
		return {};
	}
} // namespace gg
