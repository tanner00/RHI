#pragma once

#include "RHI/AccelerationStructure.hpp"

#include "Luft/NoCopy.hpp"

namespace RHI::D3D12
{

class AccelerationStructure final : public AccelerationStructureDescription, NoCopy
{
public:
	AccelerationStructure(const AccelerationStructureDescription& description, Device* device);

	uint32 HeapIndex;
};

}
