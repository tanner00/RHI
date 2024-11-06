#pragma once

#include "Common.hpp"

#include "Luft/HashTable.hpp"

enum class SamplerAddress
{
	Wrap,
	Mirror,
	Clamp,
	Border,
};

enum class SamplerFilter
{
	Point,
	Linear,
	Anisotropic,
};

struct SamplerDescription
{
	SamplerAddress Address;
	SamplerFilter Filter;
	Float4 BorderFilterColor;
};

class Sampler final : public RHI_HANDLE(Sampler)
{
public:
	RHI_HANDLE_BODY(Sampler);

	SamplerAddress GetAddress() const { return Description.Address; }
	SamplerFilter GetFilter() const { return Description.Filter; }
	Float4 GetBorderFilterColor() const { return Description.BorderFilterColor; }

private:
	SamplerDescription Description;
};

template<>
struct Hash<Sampler>
{
	uint64 operator()(const Sampler& sampler) const
	{
		const usize handle = sampler.Get();
		return HashFnv1a(&handle, sizeof(handle));
	}
};

inline bool operator==(const Sampler& a, const Sampler& b)
{
	return a.Get() == b.Get();
}

class D3D12Sampler
{
public:
	D3D12Sampler(ID3D12Device11* device, const Sampler& sampler, ViewHeap* samplerViewHeap);

	uint32 GetHeapIndex() const { CHECK(HeapIndex); return HeapIndex; }

	uint32 HeapIndex;
};
