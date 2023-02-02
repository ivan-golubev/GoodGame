module;
#include <string>
#include <memory>
module ModelD3D12;

import Model;

namespace gg
{
	ModelD3D12::ModelD3D12(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram> s, std::shared_ptr<Texture> t)
		: Model{ modelRelativePath, std::move(s), t }
	{
	}

} //namespace gg
