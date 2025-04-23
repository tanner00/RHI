#include "BufferView.hpp"
#include "Convert.hpp"
#include "Device.hpp"
#include "Resource.hpp"

namespace RHI::D3D12
{

BufferView::BufferView(const BufferViewDescription& description, D3D12::Device* device)
	: BufferViewDescription(description)
	, Device(device)
{
	CHECK(Resource.IsValid());
	const D3D12::Resource* backendResource = Resource.Backend;

	switch (Type)
	{
	case ViewType::ConstantBuffer:
	{
		HeapIndex = device->ConstantBufferShaderResourceUnorderedAccessViewHeap.AllocateIndex();

		const D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferDescription = To(description, backendResource);
		device->Native->CreateConstantBufferView(&constantBufferDescription, GetCpu());
		break;
	}
	case ViewType::ShaderResource:
	{
		HeapIndex = device->ConstantBufferShaderResourceUnorderedAccessViewHeap.AllocateIndex();

		const D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceDescription = To<ViewType::ShaderResource>(description);
		device->Native->CreateShaderResourceView(backendResource->Native, &shaderResourceDescription, GetCpu());
		break;
	}
	case ViewType::UnorderedAccess:
	{
		HeapIndex = device->ConstantBufferShaderResourceUnorderedAccessViewHeap.AllocateIndex();

		const D3D12_UNORDERED_ACCESS_VIEW_DESC unorderedAccessDescription = To<ViewType::UnorderedAccess>(description);
		device->Native->CreateUnorderedAccessView(backendResource->Native, nullptr, &unorderedAccessDescription, GetCpu());
		break;
	}
	default:
		CHECK(false);
		break;
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE BufferView::GetCpu() const
{
	return Device->GetCpu(HeapIndex, Type);
}

D3D12_GPU_DESCRIPTOR_HANDLE BufferView::GetGpu() const
{
	return Device->GetGpu(HeapIndex, Type);
}

}
