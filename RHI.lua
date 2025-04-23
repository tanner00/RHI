include "Common.lua"

project "RHI"
	kind "StaticLib"

	SetConfigurationSettings()
	UseWindowsSettings("RHI_D3D12=1")

	includedirs { "Source", "ThirdParty", "../Luft/Source" }
	files { "Source/RHI/**.cpp", "Source/RHI/**.hpp", "ThirdParty/**.h", "ThirdParty/**.hpp" }

	filter {}
