#include "Resource.hpp"
#include "Base.hpp"
#include "Convert.hpp"
#include "Device.hpp"
#include "Heap.hpp"

namespace RHI::D3D12
{

static constexpr const DXGI_FORMAT* NoCastableFormats = nullptr;

static constexpr D3D12_HEAP_PROPERTIES DefaultHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_DEFAULT,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

static constexpr D3D12_HEAP_PROPERTIES UploadHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_UPLOAD,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

static constexpr D3D12_HEAP_PROPERTIES ReadBackHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_READBACK,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

static ID3D12Resource2* AllocateResource(const Device* device, const ResourceDescription& description)
{
	const D3D12_RESOURCE_DESC1 nativeDescription = To(description);

	const D3D12_CLEAR_VALUE colorClear =
	{
		.Format = nativeDescription.Format,
		.Color = { description.ColorClear.R, description.ColorClear.G, description.ColorClear.B, description.ColorClear.A },
	};
	const D3D12_CLEAR_VALUE depthClear =
	{
		.Format = nativeDescription.Format,
		.DepthStencil = D3D12_DEPTH_STENCIL_VALUE
		{
			.Depth = description.DepthClear,
			.Stencil = 0,
		},
	};
	const D3D12_CLEAR_VALUE* clear = HasFlags(description.Flags, ResourceFlags::RenderTarget)
								   ? &colorClear
								   : (HasFlags(description.Flags, ResourceFlags::DepthStencil) ? &depthClear : nullptr);

	ID3D12Resource2* resource = nullptr;
	if (description.Allocation.Heap.IsValid())
	{
		CHECK_RESULT(device->Native->CreatePlacedResource2(description.Allocation.Heap.Backend->Native,
														   description.Allocation.Offset,
														   &nativeDescription,
														   To(description.InitialLayout),
														   clear,
														   0,
														   NoCastableFormats,
														   IID_PPV_ARGS(&resource)));
	}
	else
	{
		const D3D12_HEAP_PROPERTIES *heapProperties = HasFlags(description.Flags, ResourceFlags::Upload)
													? &UploadHeapProperties
													: (HasFlags(description.Flags, ResourceFlags::ReadBack) ? &ReadBackHeapProperties
																											: &DefaultHeapProperties);

		CHECK_RESULT(device->Native->CreateCommittedResource3(heapProperties,
															  D3D12_HEAP_FLAG_NONE,
															  &nativeDescription,
															  To(description.InitialLayout),
															  clear,
															  nullptr,
															  0,
															  NoCastableFormats,
															  IID_PPV_ARGS(&resource)));
	}
	CHECK(resource);
	SET_D3D_NAME(resource, description.Name);

	return resource;
}

Resource::Resource(const ResourceDescription& description, D3D12::Device* device)
	: ResourceDescription(description)
	, Device(device)
{
	if (HasFlags(Flags, ResourceFlags::SwapChain))
	{
		Native = Device->GetSwapChainResource(SwapChainIndex);
	}
	else
	{
		Native = AllocateResource(device, description);
	}
}

Resource::~Resource()
{
	SAFE_RELEASE(Native);
}

void Resource::Write(const ResourceDescription& format, const void* data)
{
	CHECK(data);
	CHECK(HasFlags(Flags, ResourceFlags::Upload));

	switch (format.Type)
	{
	case ResourceType::Buffer:
		WriteBuffer(format, data);
		break;
	case ResourceType::Texture2D:
		WriteTexture(format, data);
		break;
	case ResourceType::AccelerationStructureInstances:
		WriteAccelerationStructureInstances(format, data);
		break;
	default:
		CHECK(false);
	}
}

void Resource::WriteBuffer(const ResourceDescription& format, const void* data)
{
	void* mapped = nullptr;
	CHECK_RESULT(Native->Map(0, &ReadNothing, &mapped));
	Platform::MemoryCopy(mapped, data, format.Size);
	Native->Unmap(0, WriteEverything);
}

void Resource::WriteTexture(const ResourceDescription& format, const void* data)
{
	const D3D12_RESOURCE_DESC1 nativeDescription = To(format);

	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(Allocator);
	Array<uint32> rowCounts(Allocator);
	Array<uint64> rowSizes(Allocator);

	const uint16 count = format.MipMapCount != 0 ? format.MipMapCount : 1;
	layouts.GrowToLengthUninitialized(count);
	rowSizes.GrowToLengthUninitialized(count);
	rowCounts.GrowToLengthUninitialized(count);

	uint64 totalSize = 0;
	Device->Native->GetCopyableFootprints1(&nativeDescription, 0, count, 0, layouts.GetData(), rowCounts.GetData(), rowSizes.GetData(), &totalSize);

	usize dataOffset = 0;

	void* mapped = nullptr;
	CHECK_RESULT(Native->Map(0, &ReadNothing, &mapped));
	for (uint16 mipMapIndex = 0; mipMapIndex < format.MipMapCount; ++mipMapIndex)
	{
		const uint16 subresourceIndex = mipMapIndex;

		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[subresourceIndex];
		const uint64 rowSize = rowSizes[subresourceIndex];
		const uint32 rowCount = rowCounts[subresourceIndex];

		for (usize row = 0; row < rowCount; ++row)
		{
			Platform::MemoryCopy
			(
				static_cast<uint8*>(mapped) + layout.Offset + row * layout.Footprint.RowPitch,
				static_cast<const uint8*>(data) + dataOffset + row * rowSize,
				rowSize
			);
		}

		dataOffset += rowSize * rowCount;
	}
	Native->Unmap(0, WriteEverything);
}

void Resource::WriteAccelerationStructureInstances(const ResourceDescription& format, const void* data)
{
	const usize stride = Device->GetAccelerationStructureInstanceSize();
	const usize count = format.Size / stride;
	CHECK((format.Size % stride) == 0);

	void* mapped = nullptr;
	CHECK_RESULT(Native->Map(0, &ReadNothing, &mapped));
	for (usize instanceIndex = 0; instanceIndex < count; ++instanceIndex)
	{
		const AccelerationStructureInstance* instances = static_cast<const AccelerationStructureInstance*>(data);
		const AccelerationStructureInstance& instance = instances[instanceIndex];

		const D3D12_RAYTRACING_INSTANCE_DESC instanceDescription = To(instance);

		Platform::MemoryCopy(static_cast<uint8*>(mapped) + instanceIndex * stride, &instanceDescription, stride);
	}
	Native->Unmap(0, WriteEverything);
}

void Resource::Copy(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const
{
	switch (Type)
	{
	case ResourceType::Buffer:
		CopyBuffer(commandList, source);
		break;
	case ResourceType::Texture2D:
		CopyTexture(commandList, source);
		break;
	case ResourceType::AccelerationStructureInstances:
		CopyAccelerationStructureInstances(commandList, source);
		break;
	default:
		CHECK(false);
	}
}

void Resource::CopyBuffer(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const
{
	commandList->CopyResource(Native, source);
}

void Resource::CopyTexture(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const
{
	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(Allocator);

	const uint16 count = MipMapCount != 0 ? MipMapCount : 1;
	layouts.GrowToLengthUninitialized(count);

	const D3D12_RESOURCE_DESC1 nativeDescription = To(*this);
	Device->Native->GetCopyableFootprints1(&nativeDescription, 0, count, 0, layouts.GetData(), nullptr, nullptr, nullptr);

	for (usize subresourceIndex = 0; subresourceIndex < count; ++subresourceIndex)
	{
		const D3D12_TEXTURE_COPY_LOCATION sourceLocation =
		{
			.pResource = source,
			.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint = layouts[subresourceIndex],
		};
		const D3D12_TEXTURE_COPY_LOCATION destinationLocation =
		{
			.pResource = Native,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = static_cast<uint32>(subresourceIndex),
		};
		commandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
	}
}

void Resource::CopyAccelerationStructureInstances(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const
{
	commandList->CopyResource(Native, source);
}

}
