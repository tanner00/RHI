#include "Buffer.hpp"
#include "ViewHeap.hpp"
#include "PrivateCommon.hpp"

#include "D3D12/d3d12.h"

BufferResource AllocateBuffer(ID3D12Device11* device, usize size, bool upload, StringView name)
{
	const D3D12_RESOURCE_DESC1 bufferDescription =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = DefaultSampleDescription,
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
		.SamplerFeedbackMipRegion = {},
	};

	BufferResource resource = nullptr;
	CHECK_RESULT(device->CreateCommittedResource3(upload ? &UploadHeapProperties : &DefaultHeapProperties, D3D12_HEAP_FLAG_NONE,
												  &bufferDescription, ToD3D12(BarrierLayout::Undefined), nullptr, nullptr, 0, NoCastableFormats,
												  IID_PPV_ARGS(&resource)));
	SET_D3D_NAME(resource, name);
	return resource;
}

D3D12Buffer::D3D12Buffer(ID3D12Device11* device, ViewHeap* constantBufferShaderResourceUnorderedAccessViewHeap, const Buffer& buffer, StringView name)
{
	CHECK(buffer.GetSize() != 0);

	const usize resourceCount = buffer.IsStream() ? FramesInFlight : 1;
	for (usize i = 0; i < FramesInFlight; ++i)
	{
		const bool validResource = i < resourceCount;
		if (validResource)
		{
			Resource[i] = AllocateBuffer(device, buffer.GetSize(), buffer.IsStream(), name);

			HeapIndex[i] = (buffer.GetType() != BufferType::VertexBuffer) ? constantBufferShaderResourceUnorderedAccessViewHeap->AllocateIndex() : 0;

			if (buffer.GetType() == BufferType::ConstantBuffer)
			{
				const D3D12_CONSTANT_BUFFER_VIEW_DESC viewDescription =
				{
					.BufferLocation = Resource[i]->GetGPUVirtualAddress(),
					.SizeInBytes = static_cast<uint32>(buffer.GetSize()),
				};
				device->CreateConstantBufferView
				(
					&viewDescription,
					D3D12_CPU_DESCRIPTOR_HANDLE { constantBufferShaderResourceUnorderedAccessViewHeap->GetCpu(HeapIndex[i]) }
				);
			}
			else if (buffer.GetType() == BufferType::StructuredBuffer)
			{
				const D3D12_SHADER_RESOURCE_VIEW_DESC viewDescription =
				{
					.Format = DXGI_FORMAT_UNKNOWN,
					.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
					.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					.Buffer =
					{
						.FirstElement = 0,
						.NumElements = static_cast<uint32>(buffer.GetCount()),
						.StructureByteStride = static_cast<uint32>(buffer.GetStride()),
						.Flags = D3D12_BUFFER_SRV_FLAG_NONE,
					},
				};
				device->CreateShaderResourceView
				(
					Resource[i],
					&viewDescription,
					D3D12_CPU_DESCRIPTOR_HANDLE { constantBufferShaderResourceUnorderedAccessViewHeap->GetCpu(HeapIndex[i]) }
				);
			}
		}
		else
		{
			Resource[i] = nullptr;
			HeapIndex[i] = 0;
		}
	}
}

D3D12Buffer::D3D12Buffer(ID3D12Device11* device,
						 ViewHeap* constantBufferShaderResourceUnorderedAccessViewHeap,
						 const Buffer& buffer,
						 const void* staticData,
						 Array<UploadPair<Buffer>>* pendingBufferUploads,
						 StringView name)
	: D3D12Buffer(device, constantBufferShaderResourceUnorderedAccessViewHeap, buffer, name)
{
	CHECK(buffer.IsStatic());

	const BufferResource uploadResource = AllocateBuffer(device, buffer.GetSize(), true, "Upload [Buffer]"_view);
	pendingBufferUploads->Add({ uploadResource, buffer });

	void* mapped = nullptr;
	CHECK_RESULT(uploadResource->Map(0, &ReadNothing, &mapped));
	Platform::MemoryCopy(mapped, staticData, buffer.GetSize());
	uploadResource->Unmap(0, &WriteNothing);
}

void D3D12Buffer::Write(ID3D12Device11* device, const Buffer& buffer, const void* data, usize frameIndex,
						Array<UploadPair<Buffer>>* pendingBufferUploads) const
{
	CHECK(!buffer.IsStatic());

	BufferResource resource = nullptr;
	if (buffer.IsStream())
	{
		resource = GetBufferResource(frameIndex, true);
	}
	else if (buffer.IsDynamic())
	{
		resource = AllocateBuffer(device, buffer.GetSize(), true, "Upload [Buffer]"_view);
		pendingBufferUploads->Add(UploadPair<Buffer>
		{
			.Source = resource,
			.Destination = buffer,
		});
	}

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, &ReadNothing, &mapped));
	Platform::MemoryCopy(mapped, data, buffer.GetSize());
	resource->Unmap(0, WriteEverything);
}
