module;
#include <string>
#include <memory>
#include <DirectXMath.h>
module ModelD3D12;

import Model;

using DirectX::XMVECTOR;

namespace gg
{
	ModelD3D12::ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, XMVECTOR& position)
		: Model{ modelRelativePath, s, position }
	{
	}

} //namespace gg
