#include "Heap.hpp"
#include "Convert.hpp"

namespace RHI::D3D12
{

Heap::Heap(const HeapDescription& description, const D3D12::Device* device)
	: HeapDescription(description)
{
	const D3D12_HEAP_DESC nativeDescription = To(description);
	CHECK_RESULT(device->Native->CreateHeap1(&nativeDescription, nullptr, IID_PPV_ARGS(&Native)));
}

Heap::~Heap()
{
	SAFE_RELEASE(Native);
}

}
