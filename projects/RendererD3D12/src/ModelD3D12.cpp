module;
#include <string>
#include <memory>
module ModelD3D12;

import Model;

namespace gg
{
	ModelD3D12::ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s)
		: Model{ modelRelativePath, s}
	{
	}

} //namespace gg
