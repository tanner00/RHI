include "Common.lua"

project "RHI"
	kind "StaticLib"

	SetConfigurationSettings()
	UseWindowsSettings()

	includedirs {
		"Source",
		"../Luft/Source",
		"../ThirdParty",
	}
	files { "Source/RHI/**.cpp", "Source/RHI/**.hpp" }

	filter {}
