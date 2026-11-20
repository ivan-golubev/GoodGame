module;
#include <expected>
#include <string>
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
		VulkanShaderFileReadFailed,
		VulkanShaderFileEmpty,
		VulkanShaderModuleCreationFailed,
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
		D3D12DebugOverlayInitializationFailed,
		VulkanRendererInitializationFailed,
		VulkanRendererFrameFailed,
		VulkanResourceCreationFailed,
		VulkanCommandFailed,
	};

	struct Error
	{
		ErrorCode   code;
		std::string message;
	};

	void BreakIfFalse(bool);
} // namespace gg
