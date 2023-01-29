module;
#include <memory>
#include <string>
export module ModelLoader;

import Model;

export namespace gg
{
	void LoadMeshes(std::string const& modelRelativePath, std::shared_ptr<Model>);

} // namespace gg
