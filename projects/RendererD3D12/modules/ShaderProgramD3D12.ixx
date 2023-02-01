module;
#include <directx/d3dcommon.h>
#include <string>
#include <wrl.h>
export module ShaderProgramD3D12;

import ShaderProgram;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class ShaderProgramD3D12 : public ShaderProgram
	{
	public:
		ShaderProgramD3D12(ShaderProgramD3D12&&) noexcept;
		ShaderProgramD3D12& operator=(ShaderProgramD3D12&&) noexcept;

		/* Prohibit copying to make sure the underlying shader is destroyed exactly once */
		ShaderProgramD3D12(ShaderProgramD3D12 const&) = delete;
		ShaderProgramD3D12& operator=(ShaderProgramD3D12 const&) = delete;

		~ShaderProgramD3D12() noexcept = default;

		ComPtr<ID3DBlob> GetVertexShader() const;
		ComPtr<ID3DBlob> GetFragmentShader() const;

	private:
		/* Only D3D12 Renderer can create ShaderProgramD3D12 */
		friend class RendererD3D12;
		ShaderProgramD3D12() = default;
		ShaderProgramD3D12(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

		ComPtr<ID3DBlob> vertexShaderBlob;
		ComPtr<ID3DBlob> fragmentShaderBlob;
	};

} //namespace gg
