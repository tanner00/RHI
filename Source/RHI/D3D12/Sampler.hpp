#pragma once

#include "RHI/Sampler.hpp"

#include "Luft/NoCopy.hpp"

namespace RHI::D3D12
{

class Sampler final : public SamplerDescription, NoCopy
{
public:
	Sampler(const SamplerDescription& description, Device* device);

	uint32 HeapIndex;
};

}
