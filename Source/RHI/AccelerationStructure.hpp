#pragma once

#include "Buffer.hpp"
#include "Resource.hpp"

#include "Luft/Math.hpp"

namespace RHI
{

struct AccelerationStructureGeometry
{
	SubBuffer VertexBuffer;
	SubBuffer IndexBuffer;
	bool Translucent;
};

struct AccelerationStructureInstance
{
	uint32 ID;
	Matrix Transform;
	Resource AccelerationStructureResource;
};

struct AccelerationStructureSize
{
	usize ResultSize;
	usize ScratchSize;
};

struct AccelerationStructureDescription
{
	Resource AccelerationStructureResource;
};

class AccelerationStructure final : public AccelerationStructureDescription
{
public:
	AccelerationStructure()
		: AccelerationStructureDescription()
		, Backend(nullptr)
	{
	}

	AccelerationStructure(const AccelerationStructureDescription& description, RHI_BACKEND(AccelerationStructure)* backend)
		: AccelerationStructureDescription(description)
		, Backend(backend)
	{
	}

	static AccelerationStructure Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(AccelerationStructure)* Backend;
};

}
