module;
#include <glm/vec3.hpp>
#include <memory>
#include <string>
module ModelD3D12;

import Model;


namespace gg
{
	ModelD3D12::ModelD3D12(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram> s, glm::vec3& position)
		: Model{ modelRelativePath, s, position }
	{
	}

} //namespace gg
