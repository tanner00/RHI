#pragma once

#include "Device.hpp"

#include "RHI/Heap.hpp"

namespace RHI::D3D12
{

class Heap final : public HeapDescription, NoCopy
{
public:
	Heap(const HeapDescription& description, const Device* device);
	~Heap();

	ID3D12Heap1* Native;
};

}
