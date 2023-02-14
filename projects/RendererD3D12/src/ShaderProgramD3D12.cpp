module;
#include <d3dcompiler.h>
#include <filesystem>
#include <string>
module ShaderProgramD3D12;

import ShaderProgram;
import ErrorHandling;

using Microsoft::WRL::ComPtr;

namespace gg
{

	ShaderProgramD3D12::ShaderProgramD3D12(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
	{
		std::wstring vertexShaderAbsPath{ std::filesystem::absolute(vertexShaderRelativePath).generic_wstring() };
		std::wstring fragmentShaderAbsPath{ std::filesystem::absolute(fragmentShaderRelativePath).generic_wstring() };
		ThrowIfFailed(D3DReadFileToBlob(vertexShaderAbsPath.data(), &vertexShaderBlob));
		ThrowIfFailed(D3DReadFileToBlob(fragmentShaderAbsPath.data(), &fragmentShaderBlob));
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
