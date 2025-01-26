#pragma once

#include "Common.hpp"

#include "Luft/Hash.hpp"

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
	SamplerFilter MinificationFilter;
	SamplerFilter MagnificationFilter;
	SamplerAddress HorizontalAddress;
	SamplerAddress VerticalAddress;
	Float4 BorderColor;
};

class Sampler final : public RhiHandle<Sampler>
{
public:
	Sampler()
		: RhiHandle(0)
		, Description()
	{
	}

	Sampler(const RhiHandle& handle, SamplerDescription&& Description)
		: RhiHandle(handle)
		, Description(Move(Description))
	{
	}

	SamplerFilter GetMinificationFilter() const { return Description.MinificationFilter; }
	SamplerFilter GetMagnificationFilter() const { return Description.MagnificationFilter; }

	SamplerAddress GetHorizontalAddress() const { return Description.HorizontalAddress; }
	SamplerAddress GetVerticalAddress() const { return Description.VerticalAddress; }

	Float4 GetBorderColor() const { return Description.BorderColor; }

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
