# Get PowerShell 7.2.1 or higher from the MS Store: https://aka.ms/Install-PowerShell
Param(
    [Parameter(Mandatory=$true)][string]$Config, 
    [Parameter(Mandatory=$true)][string]$TargetDir, 
    [Parameter(Mandatory=$true)][string]$RendererType,
    [Parameter(Mandatory=$false)][string]$WinDxcPath
)

# Disable ANSI escape sequences so Visual Studio output window remains clean
$PSStyle.OutputRendering = 'PlainText'
$env:NO_COLOR = '1'

$ErrorActionPreference = "Stop"

$IsFinal=$($Config -eq "Final")

if ($RendererType -eq "vulkan") 
{
	# Vulkan SDK Environment variable is set via VulkanSDK installer (https://vulkan.lunarg.com)
	$global:Compiler = "${Env:VULKAN_SDK}\Bin\dxc.exe"
	$global:OutputDir = "${TargetDir}\shaders\spirv"
	$global:ShaderModel = "6_0"
	$global:Extention = "spv"
} 
else 
{ # D3D12
	# Windows SDK
	$global:Compiler = "${WinDxcPath}\dxc.exe"
	$global:OutputDir = "${TargetDir}\shaders\dxil"
	$global:ShaderModel = "6_0"
	$global:Extention = "cso"
}

if (!(Test-Path -Path $OutputDir))
{
	New-Item -Path $OutputDir -ItemType Directory
}
$OutputDir=Resolve-Path $OutputDir

#compile shaders
foreach($file in Get-ChildItem -Path $PSScriptRoot -Filter *.hlsl) {
	$Entry=[System.IO.Path]::GetFileNameWithoutExtension($file)

	if ($RendererType -eq "vulkan") 
	{
		$global:AdditionalParamsVS = "-fspv-target-env=vulkan1.3", "-spirv", "-D VULKAN"
		$global:AdditionalParamsPS = "-fspv-target-env=vulkan1.3", "-spirv", "-D VULKAN"
	} else
	{ # D3D12
		$global:AdditionalParamsVS = "-Fd", "${OutputDir}\${Entry}_VS.pdb"
		$global:AdditionalParamsPS = "-Fd", "${OutputDir}\${Entry}_PS.pdb"
	}

	if($IsFinal) {
		$FinalParams = "-Qstrip_debug"
		$AdditionalParamsPS += , $FinalParams		
	} else {
		$DebugParams = "-Od", "-Zi"
		$AdditionalParamsPS += , $DebugParams
		$AdditionalParamsVS += , $DebugParams
	}

	& $Compiler -T vs_${ShaderModel} -E vs_main @AdditionalParamsVS $file -Fo ${OutputDir}\${Entry}_VS.${Extention}
	& $Compiler -T ps_${ShaderModel} -E ps_main @AdditionalParamsPS $file -Fo ${OutputDir}\${Entry}_PS.${Extention}
}
