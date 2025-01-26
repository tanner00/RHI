#pragma once

#include "Common.hpp"

#include "Luft/Hash.hpp"

enum class ShaderStage
{
	Vertex,
	Pixel,
	Compute,
};

template<>
struct Hash<ShaderStage>
{
	uint64 operator()(ShaderStage stage) const
	{
		return HashFnv1a(&stage, sizeof(stage));
	}
};

struct ShaderDescription
{
	ShaderStage Stage;
	StringView FilePath;
};

class Shader final : public RhiHandle<Shader>
{
public:
	Shader()
		: RhiHandle(0)
		, Description()
	{
	}

	Shader(const RhiHandle& handle, ShaderDescription&& Description)
		: RhiHandle(handle)
		, Description(Move(Description))
	{
	}

	ShaderStage GetStage() const { return Description.Stage; }
	StringView GetFilePath() const { return Description.FilePath; }

private:
	ShaderDescription Description;
};

HASH_RHI_HANDLE(Shader);

class D3D12Shader
{
public:
	explicit D3D12Shader(const Shader& shader);

	IDxcBlob* Blob;
	ID3D12ShaderReflection* Reflection;
};

namespace Dxc
{
void Init();
void Shutdown();
}
