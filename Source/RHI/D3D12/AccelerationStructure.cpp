#include "AccelerationStructure.hpp"
#include "Convert.hpp"
#include "Device.hpp"

namespace RHI::D3D12
{

AccelerationStructure::AccelerationStructure(const AccelerationStructureDescription& description, Device* device)
	: AccelerationStructureDescription(description)
	, HeapIndex(device->ConstantBufferShaderResourceUnorderedAccessViewHeap.AllocateIndex())
{
	const D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceDescription = To(description);
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { device->ConstantBufferShaderResourceUnorderedAccessViewHeap.GetCpu(HeapIndex) };
	device->Native->CreateShaderResourceView(nullptr, &shaderResourceDescription, cpu);
}

}
