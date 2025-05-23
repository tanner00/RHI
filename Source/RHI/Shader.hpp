#pragma once

#include "Forward.hpp"

#include "Luft/Array.hpp"
#include "Luft/Hash.hpp"

namespace RHI
{

enum class ShaderStage : uint8
{
	Vertex,
	Pixel,
	Compute,
};

struct ShaderDefine
{
	StringView Name;
	StringView Value;
};

struct ShaderDescription
{
	StringView FilePath;
	ShaderStage Stage;
	ArrayView<ShaderDefine> Defines;
};

class Shader final : public ShaderDescription
{
public:
	Shader()
		: ShaderDescription()
		, Backend(nullptr)
	{
	}

	Shader(const ShaderDescription& description, RHI_BACKEND(Shader)* backend)
		: ShaderDescription(description)
		, Backend(backend)
	{
	}

	static Shader Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(Shader)* Backend;
};

}

template<>
struct Hash<RHI::ShaderStage>
{
	uint64 operator()(RHI::ShaderStage key) const
	{
		return HashFnv1a(&key, sizeof(key));
	}
};

