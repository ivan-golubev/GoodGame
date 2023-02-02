module;
#include <string>
export module ModelLoader;

import Model;

export namespace gg
{
	void LoadMeshes(std::string const& modelRelativePath, Model&);

} // namespace gg
