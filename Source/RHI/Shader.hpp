#pragma once

#include "Common.hpp"

#include "Luft/HashTable.hpp"

enum class ShaderStage
{
	Vertex,
	Pixel,
	Count,
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

class Shader final : public RHI_HANDLE(Shader)
{
public:
	RHI_HANDLE_BODY(Shader);

	ShaderStage GetStage() const { return Description.Stage; }
	StringView GetFilePath() const { return Description.FilePath; }

private:
	ShaderDescription Description;
};

template<>
struct Hash<Shader>
{
	uint64 operator()(const Shader& shader) const
	{
		const usize handle = shader.Get();
		return HashFnv1a(&handle, sizeof(handle));
	}
};

inline bool operator==(const Shader& a, const Shader& b)
{
	return a.Get() == b.Get();
}

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
