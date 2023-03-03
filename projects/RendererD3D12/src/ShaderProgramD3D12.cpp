module;
#include <d3dcompiler.h>
#include <directx/d3dx12_pipeline_state_stream.h>
#include <d3d12shader.h>    // Shader reflection
#include <dxcapi.h>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <bit>
module ShaderProgramD3D12;

import ShaderProgram;
import ErrorHandling;

using Microsoft::WRL::ComPtr;

namespace
{
	constexpr BYTE ComponentMaskX = 1;
	constexpr BYTE ComponentMaskXY = 1 << 1 | ComponentMaskX;
	constexpr BYTE ComponentMaskXYZ = 1 << 2 | ComponentMaskXY;
	constexpr BYTE ComponentMaskXYZW = 1 << 3 | ComponentMaskXYZ;

	DXGI_FORMAT ToDxgiFormat(D3D12_SIGNATURE_PARAMETER_DESC const& p)
	{
		if (p.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
		{
			switch (p.Mask)
			{
			case ComponentMaskX: return DXGI_FORMAT_R32_FLOAT;
			case ComponentMaskXY: return DXGI_FORMAT_R32G32_FLOAT;
			case ComponentMaskXYZ: return DXGI_FORMAT_R32G32B32_FLOAT;
			case ComponentMaskXYZW: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
		}
		gg::BreakIfFalse(false); /* the component type not implemented */
		return DXGI_FORMAT_UNKNOWN;
	}

	D3D12_INPUT_ELEMENT_DESC ToInputElementDesc(D3D12_SIGNATURE_PARAMETER_DESC const& p, char const* semanticName)
	{
		return { semanticName, p.SemanticIndex, ToDxgiFormat(p), 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	}
}


namespace gg
{
	ShaderProgramD3D12::ShaderProgramD3D12(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
	{
		std::wstring vertexShaderAbsPath{ std::filesystem::absolute(vertexShaderRelativePath).generic_wstring() };
		std::wstring fragmentShaderAbsPath{ std::filesystem::absolute(fragmentShaderRelativePath).generic_wstring() };
		ThrowIfFailed(D3DReadFileToBlob(vertexShaderAbsPath.data(), &vertexShaderBlob));
		ThrowIfFailed(D3DReadFileToBlob(fragmentShaderAbsPath.data(), &fragmentShaderBlob));

		ReflectInputLayout();
	}

	/* Do shader reflection to extract the input layout */
	void ShaderProgramD3D12::ReflectInputLayout()
	{
		ComPtr<IDxcUtils> dxcUtils;
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf())));
		DxcBuffer reflectionBuffer{};
		reflectionBuffer.Ptr = vertexShaderBlob->GetBufferPointer();
		reflectionBuffer.Size = vertexShaderBlob->GetBufferSize();
		reflectionBuffer.Encoding = 0;

		ComPtr<ID3D12ShaderReflection> shaderReflection;
		ThrowIfFailed(dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(shaderReflection.GetAddressOf())));

		D3D12_SHADER_DESC shaderDesc{};
		ThrowIfFailed(shaderReflection->GetDesc(&shaderDesc));

		/* make sure the underlying container for semantic names is not resized to keep char* valid */
		vertexInputSemantics.reserve(shaderDesc.InputParameters);
		vertexInputDesc.reserve(shaderDesc.InputParameters);

		for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC p;
			ThrowIfFailed(shaderReflection->GetInputParameterDesc(i, &p));

			vertexInputSemantics.emplace_back(p.SemanticName);
			vertexInputDesc.emplace_back(ToInputElementDesc(p, vertexInputSemantics[i].c_str()));

			{ /* save the semantic and component count of each attribute */
				Semantic semantic = semanticNameToSemantic.at(p.SemanticName);
				uint8_t componentCount = std::countr_one(p.Mask);
				inputAttributes.emplace_back(semantic, componentCount);
			}
		}
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
