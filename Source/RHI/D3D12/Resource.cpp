#include "Resource.hpp"
#include "Base.hpp"
#include "Convert.hpp"
#include "Device.hpp"

#include "Luft/Array.hpp"

namespace RHI::D3D12
{

static constexpr const DXGI_FORMAT* NoCastableFormats = nullptr;

static constexpr D3D12_HEAP_PROPERTIES UploadHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_UPLOAD,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

static constexpr D3D12_HEAP_PROPERTIES ReadbackHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_READBACK,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

static constexpr D3D12_HEAP_PROPERTIES DefaultHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_DEFAULT,
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
		.Color = { 0.0f, 0.0f, 0.0f, 1.0f },
	};
	const D3D12_CLEAR_VALUE depthClear =
	{
		.Format = nativeDescription.Format,
		.DepthStencil = D3D12_DEPTH_STENCIL_VALUE
		{
			.Depth = D3D12_MAX_DEPTH,
			.Stencil = 0,
		},
	};
	const D3D12_CLEAR_VALUE* clear = HasFlags(description.Flags, ResourceFlags::RenderTarget)
								   ? &colorClear
								   : (HasFlags(description.Flags, ResourceFlags::DepthStencil) ? &depthClear : nullptr);

	const D3D12_HEAP_PROPERTIES *heapProperties = HasFlags(description.Flags, ResourceFlags::Upload)
												? &UploadHeapProperties
												: (HasFlags(description.Flags, ResourceFlags::Readback) ? &ReadbackHeapProperties : &DefaultHeapProperties);

	ID3D12Resource2* resource = nullptr;
	CHECK_RESULT(device->Native->CreateCommittedResource3(heapProperties,
														  D3D12_HEAP_FLAG_NONE,
														  &nativeDescription,
														  To(description.InitialLayout),
														  clear,
														  nullptr,
														  0,
														  NoCastableFormats,
														  IID_PPV_ARGS(&resource)));
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
	Device->AddPendingDelete(Native);
}

void Resource::Write(const void* data, Array<UploadPair<ID3D12Resource2*, Resource*>>* pendingUploads)
{
	switch (Type)
	{
	case ResourceType::Buffer:
		WriteBuffer(data, pendingUploads);
		break;
	case ResourceType::Texture2D:
		WriteTexture(data, pendingUploads);
		break;
	case ResourceType::AccelerationStructureInstances:
		WriteAccelerationStructureInstances(data);
		break;
	default:
		CHECK(false);
	}
}

void Resource::WriteBuffer(const void* data, Array<UploadPair<ID3D12Resource2*, Resource*>>* pendingUploads)
{
	ID3D12Resource2* uploadResource = Native;

	const bool streamed = HasFlags(Flags, ResourceFlags::Upload);
	if (!streamed)
	{
		uploadResource = AllocateResource(Device,
		{
			.Type = ResourceType::Buffer,
			.Flags = ResourceFlags::Upload,
			.InitialLayout = BarrierLayout::Undefined,
			.Size = Size,
			.Name = "Upload Buffer"_view,
		});
	}

	void* mapped = nullptr;
	CHECK_RESULT(uploadResource->Map(0, &ReadNothing, &mapped));
	Platform::MemoryCopy(mapped, data, Size);
	uploadResource->Unmap(0, WriteEverything);

	if (!streamed)
		pendingUploads->Add({ uploadResource, this });
}

void Resource::WriteTexture(const void* data, Array<UploadPair<ID3D12Resource2*, Resource*>>* pendingUploads)
{
	CHECK(!HasFlags(Flags, ResourceFlags::Upload));

	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(Allocator);
	Array<uint32> rowCounts(Allocator);
	Array<uint64> rowSizes(Allocator);

	const uint16 count = MipMapCount != 0 ? MipMapCount : 1;
	layouts.GrowToLengthUninitialized(count);
	rowSizes.GrowToLengthUninitialized(count);
	rowCounts.GrowToLengthUninitialized(count);

	uint64 totalSize = 0;

	const D3D12_RESOURCE_DESC1 description = To(*this);
	Device->Native->GetCopyableFootprints1(&description, 0, count, 0, layouts.GetData(), rowCounts.GetData(), rowSizes.GetData(), &totalSize);

	ID3D12Resource2* uploadResource = AllocateResource(Device,
	{
		.Type = ResourceType::Buffer,
		.Flags = ResourceFlags::Upload,
		.InitialLayout = BarrierLayout::Undefined,
		.Size = totalSize,
		.Name = "Upload Texture"_view,
	});

	usize dataOffset = 0;

	void* mapped = nullptr;
	CHECK_RESULT(uploadResource->Map(0, &ReadNothing, &mapped));
	for (uint16 mipMapIndex = 0; mipMapIndex < MipMapCount; ++mipMapIndex)
	{
		const uint16 subresourceIndex = mipMapIndex;

		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[subresourceIndex];
		const uint64 rowSize = rowSizes[subresourceIndex];
		const uint32 rowCount = rowCounts[subresourceIndex];

		for (usize row = 0; row < rowCounts[subresourceIndex]; ++row)
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
	uploadResource->Unmap(0, WriteEverything);

	pendingUploads->Add({ uploadResource, this });
}

void Resource::WriteAccelerationStructureInstances(const void* data)
{
	CHECK(HasFlags(Flags, ResourceFlags::Upload));

	ID3D12Resource2* uploadResource = Native;

	const usize stride = Device->GetAccelerationStructureInstanceSize();
	const usize count = Size / stride;
	CHECK((Size % stride) == 0);

	void* mapped = nullptr;
	CHECK_RESULT(uploadResource->Map(0, &ReadNothing, &mapped));
	for (usize instanceIndex = 0; instanceIndex < count; ++instanceIndex)
	{
		const AccelerationStructureInstance* instances = static_cast<const AccelerationStructureInstance*>(data);
		const AccelerationStructureInstance& instance = instances[instanceIndex];

		const D3D12_RAYTRACING_INSTANCE_DESC instanceDescription = To(instance);

		Platform::MemoryCopy(static_cast<uint8*>(mapped) + instanceIndex * stride, &instanceDescription, stride);
	}
	uploadResource->Unmap(0, WriteEverything);
}

void Resource::Upload(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const
{
	switch (Type)
	{
	case ResourceType::Buffer:
		UploadBuffer(uploadCommandList, source);
		break;
	case ResourceType::Texture2D:
		UploadTexture(uploadCommandList, source);
		break;
	case ResourceType::AccelerationStructureInstances:
		UploadAccelerationStructureInstances(uploadCommandList, source);
		break;
	default:
		CHECK(false);
	}
}

void Resource::UploadBuffer(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const
{
	uploadCommandList->CopyResource(Native, source);
}

void Resource::UploadTexture(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const
{
	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(Allocator);

	const uint16 count = MipMapCount != 0 ? MipMapCount : 1;
	layouts.GrowToLengthUninitialized(count);

	const D3D12_RESOURCE_DESC1 description = To(*this);
	Device->Native->GetCopyableFootprints1(&description, 0, count, 0, layouts.GetData(), nullptr, nullptr, nullptr);

	for (usize subresourceIndex = 0; subresourceIndex < count; ++subresourceIndex)
	{
		const D3D12_TEXTURE_COPY_LOCATION sourceLocation = D3D12_TEXTURE_COPY_LOCATION
		{
			.pResource = source,
			.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint = layouts[subresourceIndex],
		};
		const D3D12_TEXTURE_COPY_LOCATION destinationLocation = D3D12_TEXTURE_COPY_LOCATION
		{
			.pResource = Native,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = static_cast<uint32>(subresourceIndex),
		};
		uploadCommandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
	}
}

void Resource::UploadAccelerationStructureInstances(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const
{
	uploadCommandList->CopyResource(Native, source);
}

}
