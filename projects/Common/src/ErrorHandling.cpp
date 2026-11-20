module;
#include <windows.h>
module ErrorHandling;

import GlobalSettings;

namespace gg
{
	void BreakIfFalse(bool condition)
	{
		if constexpr (IsDebug())
		{
			if (!condition)
				__debugbreak();
		}
	}

} // namespace gg
