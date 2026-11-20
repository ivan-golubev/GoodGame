module;
#include <memory>
export module Scene;

export import Camera;
import Input;
export import Lighting;
import SettingsRenderer;

export namespace gg
{
	/* What is being shown, independent of how it is drawn. Owned by the Application and passed to the
	   renderer each frame. */
	struct Scene
	{
		explicit Scene(std::shared_ptr<InputManager> inputManager)
			: camera{ std::move(inputManager) }
		{
		}

		Camera           camera;
		DirectionalLight light{ defaultDirectionalLight };
	};
} // namespace gg
