module;
#include <iostream>
#include <string>
#include <windows.h>
module Logging;

import GlobalSettings;

namespace
{
	using gg::DebugLevel;

	std::wstring ToWString(std::string const& str)
	{
		if (str.empty()) return {};
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
	}

	void PrintToVSDebugConsole(DebugLevel debugLevel, std::string const& message)
	{
		std::wstring wMessage{ ToWString(message) };
		wchar_t const* debugLevelStr = (debugLevel == DebugLevel::Info) ? L"[Info] " : L"[Error] ";
		OutputDebugStringW(debugLevelStr);
		OutputDebugStringW(wMessage.c_str());
		OutputDebugStringW(L"\n");
	}
}

namespace gg
{
	void DebugLog(DebugLevel debugLevel, std::string const& message) noexcept
	{
		if constexpr (IsDebug())
		{
			if constexpr (IsWindowsSubSystem())
			{
				try
				{
					PrintToVSDebugConsole(debugLevel, message);
				}
				catch (...)
				{
					// Logging must not interfere with application shutdown or error handling.
				}
			}
			else
			{
				auto& outStream = (debugLevel == DebugLevel::Info) ? std::cout : std::cerr;
				// Keep logging failures in the stream state instead of throwing.
				outStream.exceptions(std::ios::goodbit);
				outStream << message << std::endl;
			}
		}
	}

} // namespace gg
