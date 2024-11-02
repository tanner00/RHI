#pragma once

#include "Common.hpp"

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

struct D3D12Sampler
{
	usize HeapIndex;
};
