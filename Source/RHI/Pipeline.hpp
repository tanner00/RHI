#pragma once

#include "Common.hpp"
#include "Shader.hpp"

#include "Luft/HashTable.hpp"

inline constexpr usize BindingBucketCount = 4;

class Pipeline : public RhiHandle<Pipeline>
{
public:
	explicit Pipeline(const RhiHandle& handle)
		: RhiHandle(handle)
	{
	}
};

template<>
struct Hash<Pipeline>
{
	uint64 operator()(const Pipeline& pipeline) const
	{
		const usize handle = pipeline.Get();
		return HashFnv1a(&handle, sizeof(handle));
	}
};

inline bool operator==(const Pipeline& a, const Pipeline& b)
{
	return a.Get() == b.Get();
}

enum class PipelineType
{
	Graphics,
	Compute,
};

struct RootParameter
{
	usize Index;
	usize Size;
};

class D3D12Pipeline
{
public:
	explicit D3D12Pipeline(PipelineType type)
		: RootParameters(BindingBucketCount, &GlobalAllocator::Get())
		, RootSignature(nullptr)
		, PipelineState(nullptr)
		, Type(type)
	{
	}

	HashTable<String, RootParameter> RootParameters;
	ID3D12RootSignature* RootSignature;
	ID3D12PipelineState* PipelineState;
	PipelineType Type;
};
