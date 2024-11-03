#include "Buffer.hpp"
#include "BarrierConversion.hpp"

#include "D3D12/d3d12.h"

BufferResource AllocateBuffer(ID3D12Device11* device, usize size, bool upload, StringView name)
{
	const D3D12_HEAP_PROPERTIES heapProperties =
	{
		.Type = upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 0,
		.VisibleNodeMask = 0,
	};
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

	static constexpr const DXGI_FORMAT* noCastableFormats = nullptr;
	BufferResource resource = nullptr;
	CHECK_RESULT(device->CreateCommittedResource3(&heapProperties, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &bufferDescription,
												  ToD3D12(BarrierLayout::Undefined), nullptr, nullptr, 0, noCastableFormats, IID_PPV_ARGS(&resource)));
#if DEBUG
	CHECK_RESULT(resource->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.GetLength()), name.GetData()));
#else
	(void)name;
#endif
	return resource;
}

D3D12Buffer::D3D12Buffer(ID3D12Device11* device, const Buffer& buffer, StringView name)
{
	CHECK(buffer.GetSize() != 0);

	const usize resourceCount = buffer.IsStream() ? FramesInFlight : 1;
	for (usize i = 0; i < FramesInFlight; ++i)
	{
		const bool validResource = i < resourceCount;
		if (validResource)
		{
			Resources[i] = AllocateBuffer(device, buffer.GetSize(), buffer.IsStream(), name);
		}
		else
		{
			Resources[i] = nullptr;
		}
	}
}

D3D12Buffer::D3D12Buffer(ID3D12Device11* device, const Buffer& buffer, const void* staticData, Array<UploadPair<Buffer>>* pendingBufferUploads,
						 StringView name)
	: D3D12Buffer(device, buffer, name)
{
	CHECK(buffer.IsStatic());

	const BufferResource uploadResource = AllocateBuffer(device, buffer.GetSize(), true, "Upload [Buffer]"_view);
	pendingBufferUploads->Add({ uploadResource, buffer });

	void* mapped = nullptr;
	CHECK_RESULT(uploadResource->Map(0, nullptr, &mapped));
	Platform::MemoryCopy(mapped, staticData, buffer.GetSize());
	static constexpr const D3D12_RANGE* writeEverything = nullptr;
	uploadResource->Unmap(0, writeEverything);
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
	CHECK_RESULT(resource->Map(0, nullptr, &mapped));
	Platform::MemoryCopy(mapped, data, buffer.GetSize());
	static constexpr const D3D12_RANGE* writeEverything = nullptr;
	resource->Unmap(0, writeEverything);
}
