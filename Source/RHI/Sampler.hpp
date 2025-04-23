#pragma once

#include "Forward.hpp"
#include "HLSL.hpp"

namespace RHI
{

enum class SamplerAddress : uint8
{
	Wrap,
	Mirror,
	Clamp,
	Border,
};

enum class SamplerFilter : uint8
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

class Sampler final : public SamplerDescription
{
public:
	Sampler()
		: SamplerDescription()
		, Backend(nullptr)
	{
	}

	Sampler(const SamplerDescription& description, RHI_BACKEND(Sampler)* backend)
		: SamplerDescription(description)
		, Backend(backend)
	{
	}

	static Sampler Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(Sampler)* Backend;
};

}
