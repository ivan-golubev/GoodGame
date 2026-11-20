module;
#include <memory>
export module DebugOverlay;

import Renderer;
import Scene;

export namespace gg
{
	void RenderDebugOverlay(Renderer&, Scene&);
} // namespace gg
