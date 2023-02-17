module;
#include <memory>
export module DebugUI;

export namespace gg
{
	class Camera;

	void RenderDebugUI(std::shared_ptr<Camera>);
} // namespace gg
