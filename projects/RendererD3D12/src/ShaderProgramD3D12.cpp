module;
#include <d3dcompiler.h>
#include <filesystem>
#include <format>
#include <expected>
#include <string>
module ShaderProgramD3D12;

import ShaderProgram;
import ErrorHandling;

using Microsoft::WRL::ComPtr;

namespace gg
{

	std::expected<void, Error> ShaderProgramD3D12::Initialize(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
	{
		std::wstring vertexShaderAbsPath{ std::filesystem::absolute(vertexShaderRelativePath).generic_wstring() };
		std::wstring fragmentShaderAbsPath{ std::filesystem::absolute(fragmentShaderRelativePath).generic_wstring() };
		HRESULT const vertexResult{ D3DReadFileToBlob(vertexShaderAbsPath.c_str(), &vertexShaderBlob) };
		if (FAILED(vertexResult))
			return std::unexpected(Error{
				.code = ErrorCode::D3D12ShaderReadFailed,
				.message = std::format("Failed to read D3D12 vertex shader '{}': HRESULT 0x{:08X}", vertexShaderRelativePath, static_cast<uint32_t>(vertexResult)),
			});

		HRESULT const fragmentResult{ D3DReadFileToBlob(fragmentShaderAbsPath.c_str(), &fragmentShaderBlob) };
		if (FAILED(fragmentResult))
			return std::unexpected(Error{
				.code = ErrorCode::D3D12ShaderReadFailed,
				.message = std::format("Failed to read D3D12 pixel shader '{}': HRESULT 0x{:08X}", fragmentShaderRelativePath, static_cast<uint32_t>(fragmentResult)),
			});
		return {};
	}

	ShaderProgramD3D12::ShaderProgramD3D12(ShaderProgramD3D12&& other) noexcept
	{
		vertexShaderBlob.Reset();
		fragmentShaderBlob.Reset();
		std::exchange(vertexShaderBlob, other.vertexShaderBlob);
		std::exchange(fragmentShaderBlob, other.fragmentShaderBlob);
	}

	ShaderProgramD3D12& ShaderProgramD3D12::operator=(ShaderProgramD3D12&& other) noexcept
	{
		if (this != &other)
		{
			vertexShaderBlob.Reset();
			fragmentShaderBlob.Reset();
			std::exchange(vertexShaderBlob, other.vertexShaderBlob);
			std::exchange(fragmentShaderBlob, other.fragmentShaderBlob);
		}
		return *this;
	}

	ComPtr<ID3DBlob> ShaderProgramD3D12::GetVertexShader() const
	{
		return vertexShaderBlob;
	}

	ComPtr<ID3DBlob> ShaderProgramD3D12::GetFragmentShader() const
	{
		return fragmentShaderBlob;
	}
} //namespace gg
