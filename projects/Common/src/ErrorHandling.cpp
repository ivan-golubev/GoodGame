module;
#include <cstdint>
#include <format>
#include <format>
#include <string>
#include <windows.h>
module ErrorHandling;

import GlobalSettings;

namespace gg
{
	ComException::ComException(HRESULT hr)
		: returnCode{ hr }
	{
	}

	std::wstring ComException::WhatString() const
	{
		return std::format(L"Failed with HRESULT={}", static_cast<uint32_t>(returnCode));
	}

	char const* ComException::what() const
	{
		static char message[64]{};
		sprintf_s(message, "Failure with HRESULT of %08X", static_cast<unsigned int>(returnCode));
		return message;
	}

	void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
			throw ComException(hr);
	}

	void BreakIfFalse(bool condition)
	{
		if constexpr (IsDebug())
		{
			if (!condition)
				__debugbreak();
		}
	}

	ApplicationInitException::ApplicationInitException(std::string const& msg)
		: std::runtime_error{ std::format("Failed to initialize the game: {}.", msg) }
	{
	}

	AssetLoadException::AssetLoadException(std::string const& msg)
		: std::runtime_error{ std::format("Failed to load an asset: {}.", msg) }
	{
	}

} // namespace gg
