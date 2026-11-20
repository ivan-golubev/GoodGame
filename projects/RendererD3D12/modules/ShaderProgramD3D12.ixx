module;
#include <directx/d3dcommon.h>
#include <expected>
#include <string>
#include <wrl.h>
export module ShaderProgramD3D12;

import ShaderProgram;
export import ErrorHandling;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class ShaderProgramD3D12 : public ShaderProgram
	{
	public:
		/* Only RendererD3D12 can make a PassKey, so only it can create a ShaderProgramD3D12,
		   while std::make_unique can still call the public constructor. */
		class PassKey
		{
			friend class RendererD3D12;
			PassKey() = default;
		};
		explicit ShaderProgramD3D12(PassKey) {}

		ShaderProgramD3D12(ShaderProgramD3D12&&) noexcept;
		ShaderProgramD3D12& operator=(ShaderProgramD3D12&&) noexcept;

		/* Prohibit copying to make sure the underlying shader is destroyed exactly once */
		ShaderProgramD3D12(ShaderProgramD3D12 const&)            = delete;
		ShaderProgramD3D12& operator=(ShaderProgramD3D12 const&) = delete;

		~ShaderProgramD3D12() noexcept = default;

		ComPtr<ID3DBlob> GetVertexShader() const;
		ComPtr<ID3DBlob> GetFragmentShader() const;

	private:
		/* Only RendererD3D12 initializes the shaders */
		friend class RendererD3D12;
		std::expected<void, Error> Initialize(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

		ComPtr<ID3DBlob> vertexShaderBlob;
		ComPtr<ID3DBlob> fragmentShaderBlob;
	};

} // namespace gg
