include "Common.lua"

project "RHI"
	kind "StaticLib"

	SetConfigurationSettings()
	UseWindowsSettings()

	includedirs { "Source" }
	files { "Source/**.cpp", "Source/**.hpp" }

	filter {}
