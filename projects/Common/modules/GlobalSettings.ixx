export module GlobalSettings;

export namespace gg 
{
    inline consteval bool IsDebug() 
    {
#ifdef _DEBUG
        return true;
#else
        return false;
#endif
    }

    inline consteval bool IsFinal()
    {
#ifdef FINAL
        return true;
#else
        return false;
#endif
    }

    inline consteval bool IsWindowsSubSystem()
    {
#ifdef _CONSOLE
        return false;
#else
        return true;
#endif
    }
} // namespace gg
