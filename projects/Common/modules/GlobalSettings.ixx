module;
export module GlobalSettings;

export namespace gg
{
	consteval bool IsDebug()
	{
#ifdef _DEBUG
		return true;
#else
		return false;
#endif
	}

	consteval bool IsFinal()
	{
#ifdef FINAL
		return true;
#else
		return false;
#endif
	}

	consteval bool IsWindowsSubSystem()
	{
#ifdef _CONSOLE
		return false;
#else
		return true;
#endif
	}
} // namespace gg
