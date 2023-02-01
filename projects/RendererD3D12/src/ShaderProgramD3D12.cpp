module;
#include <string>
#include <d3dcompiler.h>
module ShaderProgramD3D12;

import ShaderProgram;
import ErrorHandling;

using Microsoft::WRL::ComPtr;

namespace gg
{

	ShaderProgramD3D12::ShaderProgramD3D12(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
	{
		// TODO: read the textured surface shader later
		ThrowIfFailed(D3DReadFileToBlob(L"shaders//colored_surface_VS.cso", &vertexShaderBlob));
		ThrowIfFailed(D3DReadFileToBlob(L"shaders//colored_surface_PS.cso", &fragmentShaderBlob));
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
