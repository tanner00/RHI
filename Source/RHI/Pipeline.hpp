#pragma once

#include "Common.hpp"
#include "Shader.hpp"

#include "Luft/HashTable.hpp"

inline constexpr usize BindingBucketCount = 4;

class Pipeline : public RhiHandle<Pipeline>
{
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

struct RootParameter
{
	usize Index;
	usize Size;
};

class D3D12Pipeline
{
public:
	D3D12Pipeline()
		: RootParameters(BindingBucketCount, &GlobalAllocator::Get())
		, RootSignature(nullptr)
		, PipelineState(nullptr)
	{
	}

	HashTable<String, RootParameter> RootParameters;
	ID3D12RootSignature* RootSignature;
	ID3D12PipelineState* PipelineState;
};
