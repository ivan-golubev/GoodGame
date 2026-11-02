module;
#include <exception>
#include <expected>
#include <stdexcept>
#include <string>
#include <windows.h>
export module ErrorHandling;

export namespace gg
{
	enum class ErrorCode
	{
		ModelImportFailed,
		ModelContainsNoMeshes,
		ModelMeshMissingPositions,
		ModelMeshMissingTextureCoordinates,
		ModelMeshMissingNormals,
		TextureLoadFailed,
		ShaderFileReadFailed,
		ShaderFileEmpty,
		ShaderModuleCreationFailed,
		VulkanTextureResourceCreationFailed,
		VulkanTextureUnsupportedLayoutTransition,
		D3D12ShaderReadFailed,
		D3D12TextureUploadFailed,
		D3D12InitializationFailed,
		D3D12PipelineCreationFailed,
		D3D12CommandRecordingFailed,
		D3D12BufferCreationFailed,
		D3D12ResizeFailed,
		D3D12PresentationFailed,
		D3D12SynchronizationFailed,
		D3D12DebugUIInitializationFailed,
		VulkanRendererInitializationFailed,
		VulkanRendererFrameFailed,
		VulkanResourceCreationFailed,
		VulkanCommandFailed,
	};

	struct Error
	{
		ErrorCode code;
		std::string message;
	};

	class ComException : public std::exception
	{
	public:
		ComException(HRESULT hr);
		std::wstring whatString() const;
		char const* what() const override;
	private:
		HRESULT returnCode;
	};

	void ThrowIfFailed(HRESULT hr);
	void BreakIfFalse(bool);

	class ApplicationInitException : public std::runtime_error
	{
	public:
		ApplicationInitException(std::string const& msg);
	};

	class AssetLoadException : public std::runtime_error
	{
	public:
		AssetLoadException(std::string const& msg);
	};
} // namespace gg
