module;
#include <d3dcompiler.h>
#include <directx/d3dx12_pipeline_state_stream.h>
#include <filesystem>
#include <string>
#include <vector>
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

		// TODO: this should not be hardcoded, but reflected from a shader
		vertexInputDesc = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		vertexInputLayout = { vertexInputDesc.data(), static_cast<uint32_t>(vertexInputDesc.size()) };
	}

	ShaderProgramD3D12::ShaderProgramD3D12(ShaderProgramD3D12&& other) noexcept
	{
		vertexShaderBlob.Reset();
		fragmentShaderBlob.Reset();
		std::exchange(vertexShaderBlob, other.vertexShaderBlob);
		std::exchange(fragmentShaderBlob, other.fragmentShaderBlob);
		vertexInputDesc = other.vertexInputDesc;
		vertexInputLayout = other.vertexInputLayout;
	}

	ShaderProgramD3D12& ShaderProgramD3D12::operator=(ShaderProgramD3D12&& other) noexcept
	{
		if (this != &other)
		{
			vertexShaderBlob.Reset();
			fragmentShaderBlob.Reset();
			std::exchange(vertexShaderBlob, other.vertexShaderBlob);
			std::exchange(fragmentShaderBlob, other.fragmentShaderBlob);
			vertexInputDesc = other.vertexInputDesc;
			vertexInputLayout = other.vertexInputLayout;
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

	CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT& ShaderProgramD3D12::GetInputLayout()
	{
		return vertexInputLayout;
	}
} //namespace gg
