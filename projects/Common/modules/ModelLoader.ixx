module;
#include <memory>
#include <string>
export module ModelLoader;

import Model;
import ShaderProgram;
import Texture;

namespace gg
{
	export class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		[[nodiscard("Loaded model should be passed to the renderer")]]
		std::unique_ptr<Model> LoadModel(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram>, std::shared_ptr<Texture>);

	private:
		bool LoadMeshes(std::string const& modelAbsolutePath, Model& outModel);
	};

} // namespace gg