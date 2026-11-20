#pragma once

/* Returns std::unexpected(gg::Error) from the calling function when an HRESULT failed.
   Needs <format>, <expected> and the ErrorHandling module where it is used. */
#define D3D12_CHECK_HRESULT(hr, errorCode, operation)                                                                  \
	do                                                                                                                 \
	{                                                                                                                  \
		if (FAILED(hr))                                                                                                \
			return std::unexpected(gg::Error{                                                                          \
				.code    = errorCode,                                                                                  \
				.message = std::format("D3D12 {} failed with HRESULT 0x{:08X}", operation, static_cast<uint32_t>(hr)), \
			});                                                                                                        \
	} while (false)
