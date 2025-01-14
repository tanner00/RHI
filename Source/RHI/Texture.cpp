#include "Texture.hpp"
#include "Buffer.hpp"
#include "PrivateCommon.hpp"
#include "ViewHeap.hpp"

TextureResource AllocateTexture(ID3D12Device11* device, const Texture& texture, BarrierLayout initialLayout, StringView name)
{
	const D3D12_CLEAR_VALUE depthClear =
	{
		.Format = ToD3D12(texture.GetFormat()),
		.DepthStencil =
		{
			.Depth = D3D12_MAX_DEPTH,
			.Stencil = 0,
		},
	};

	TextureResource resource = nullptr;
	const D3D12_RESOURCE_DESC1 textureDescription = ToD3D12(texture);
	CHECK_RESULT(device->CreateCommittedResource3(&DefaultHeapProperties, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &textureDescription,
												  ToD3D12(initialLayout), IsDepthFormat(texture.GetFormat()) ? &depthClear : nullptr,
												  nullptr, 0, NoCastableFormats, IID_PPV_ARGS(&resource)));
	SET_D3D_NAME(resource, name);
	return resource;
}

UploadPair<Texture> WriteTexture(ID3D12Device11* device, const Texture& texture, const void* data)
{
	CHECK(texture.GetType() == TextureType::Rectangle);
	CHECK(texture.GetArrayCount() == 1);

	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(&GlobalAllocator::Get());
	Array<uint32> rowCounts(&GlobalAllocator::Get());
	Array<uint64> rowSizes(&GlobalAllocator::Get());
	uint64 totalSize = 0;

	layouts.GrowToLengthUninitialized(texture.GetMipMapCount());
	rowSizes.GrowToLengthUninitialized(texture.GetMipMapCount());
	rowCounts.GrowToLengthUninitialized(texture.GetMipMapCount());

	const D3D12_RESOURCE_DESC1 textureDescription = ToD3D12(texture);
	device->GetCopyableFootprints1(&textureDescription, 0, texture.GetMipMapCount(), 0,
								   layouts.GetData(), rowCounts.GetData(), rowSizes.GetData(), &totalSize);

	const BufferResource resource = AllocateBuffer(device, totalSize, true, "Upload [Texture]"_view);

	usize dataOffset = 0;

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, &ReadNothing, &mapped));
	for (usize subresourceIndex = 0; subresourceIndex < texture.GetMipMapCount(); ++subresourceIndex)
	{
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
	resource->Unmap(0, WriteEverything);

	return UploadPair<Texture>
	{
		.Source = resource,
		.Destination = texture,
	};
}

UploadPair<Texture> WriteCubemapTexture(ID3D12Device11* device, const Texture& texture, const Array<uint8*>& faces)
{
	CHECK(texture.GetType() == TextureType::Cubemap);
	CHECK(texture.GetArrayCount() == 6);
	CHECK(faces.GetLength() == 6);

	Array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(&GlobalAllocator::Get());
	Array<uint32> rowCounts(&GlobalAllocator::Get());
	Array<uint64> rowSizes(&GlobalAllocator::Get());
	uint64 totalSize = 0;

	layouts.GrowToLengthUninitialized(texture.GetCount());
	rowSizes.GrowToLengthUninitialized(texture.GetCount());
	rowCounts.GrowToLengthUninitialized(texture.GetCount());

	const D3D12_RESOURCE_DESC1 textureDescription = ToD3D12(texture);
	device->GetCopyableFootprints1(&textureDescription, 0, texture.GetCount(), 0,
								   layouts.GetData(), rowCounts.GetData(), rowSizes.GetData(), &totalSize);

	const BufferResource resource = AllocateBuffer(device, totalSize, true, "Upload [Cubemap Texture]"_view);

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, &ReadNothing, &mapped));
	for (usize faceIndex = 0; faceIndex < texture.GetArrayCount(); ++faceIndex)
	{
		usize dataOffset = 0;

		for (usize mipMapIndex = 0; mipMapIndex < texture.GetMipMapCount(); ++mipMapIndex)
		{
			const usize subresourceIndex = faceIndex * texture.GetMipMapCount() + mipMapIndex;

			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[subresourceIndex];
			const uint64 rowSize = rowSizes[subresourceIndex];
			const uint32 rowCount = rowCounts[subresourceIndex];

			for (usize row = 0; row < rowCounts[subresourceIndex]; ++row)
			{
				Platform::MemoryCopy
				(
					static_cast<uint8*>(mapped) + layout.Offset + row * layout.Footprint.RowPitch,
					static_cast<const uint8*>(faces[faceIndex]) + dataOffset + row * rowSize,
					rowSize
				);
			}

			dataOffset += rowSize * rowCount;
		}
	}
	resource->Unmap(0, WriteEverything);

	return UploadPair<Texture>
	{
		.Source = resource,
		.Destination = texture,
	};
}

static uint32 CreateShaderResourceView(ID3D12Device11* device, ViewHeap* shaderResourceViewHeap, TextureResource resource, const Texture& texture)
{
	const uint32 heapIndex = shaderResourceViewHeap->AllocateIndex();

	D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription;
	switch (texture.GetType())
	{
	case TextureType::Rectangle:
		shaderResourceViewDescription =
		{
			.Format = ToD3D12View(texture.GetFormat(), ViewType::ShaderResource),
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D =
			{
				.MostDetailedMip = 0,
				.MipLevels = texture.GetMipMapCount(),
				.PlaneSlice = 0,
				.ResourceMinLODClamp = 0,
			},
		};
		break;
	case TextureType::Cubemap:
		shaderResourceViewDescription =
		{
			.Format = ToD3D12View(texture.GetFormat(), ViewType::ShaderResource),
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.TextureCube =
			{
				.MostDetailedMip = 0,
				.MipLevels = texture.GetMipMapCount(),
				.ResourceMinLODClamp = 0,
			},
		};
		break;
	default:
		CHECK(false);
		break;
	}
	device->CreateShaderResourceView
	(
		resource,
		&shaderResourceViewDescription,
		D3D12_CPU_DESCRIPTOR_HANDLE { shaderResourceViewHeap->GetCpu(heapIndex) }
	);
	return heapIndex;
}

static uint32 CreateRenderTargetView(ID3D12Device11* device, ViewHeap* renderTargetViewHeap, TextureResource resource, const Texture& texture)
{
	const uint32 heapIndex = renderTargetViewHeap->AllocateIndex();

	const D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDescription =
	{
		.Format = ToD3D12View(texture.GetFormat(), ViewType::RenderTarget),
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D =
		{
			.MipSlice = 0,
			.PlaneSlice = 0,
		},
	};
	device->CreateRenderTargetView
	(
		resource,
		&renderTargetViewDescription,
		D3D12_CPU_DESCRIPTOR_HANDLE { renderTargetViewHeap->GetCpu(heapIndex) }
	);
	return heapIndex;
}

static uint32 CreateDepthStencilView(ID3D12Device11* device, ViewHeap* depthStencilViewHeap, TextureResource resource, const Texture& texture)
{
	const uint32 heapIndex = depthStencilViewHeap->AllocateIndex();

	const D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription =
	{
		.Format = ToD3D12View(texture.GetFormat(), ViewType::DepthStencil),
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Texture2D =
		{
			.MipSlice = 0,
		},
	};
	device->CreateDepthStencilView
	(
		resource,
		&depthStencilViewDescription,
		D3D12_CPU_DESCRIPTOR_HANDLE { depthStencilViewHeap->GetCpu(heapIndex) }
	);
	return heapIndex;
}


D3D12Texture::D3D12Texture(ID3D12Device11* device, ViewHeap* shaderResourceViewHeap, ViewHeap* renderTargetViewHeap, ViewHeap* depthStencilViewHeap,
						   const Texture& texture, BarrierLayout initialLayout, TextureResource existingResource, StringView name)
	: Resource(existingResource ? existingResource : AllocateTexture(device, texture, initialLayout, name))
{
	if (IsDepthFormat(texture.GetFormat()))
	{
		HeapIndex = CreateDepthStencilView(device, depthStencilViewHeap, Resource, texture);
	}
	else if (texture.IsRenderTarget())
	{
		HeapIndex = CreateRenderTargetView(device, renderTargetViewHeap, Resource, texture);
	}
	else
	{
		HeapIndex = CreateShaderResourceView(device, shaderResourceViewHeap, Resource, texture);
	}
}
