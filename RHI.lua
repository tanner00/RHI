include "Common.lua"

project "RHI"
	kind "StaticLib"

	SetConfigurationSettings()
	UseWindowsSettings()

	includedirs { "Source", "ThirdParty", "../Luft/Source" }
	files { "Source/RHI/**.cpp", "Source/RHI/**.hpp", "ThirdParty/**.h", "ThirdParty/**.hpp" }

	filter {}
