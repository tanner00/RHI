#include "Texture.hpp"
#include "BarrierConversion.hpp"
#include "Buffer.hpp"

#include "D3D12/d3d12.h"

TextureResource AllocateTexture(ID3D12Device11* device, const Texture& texture, BarrierLayout initialLayout, StringView name)
{
	const D3D12_RESOURCE_FLAGS renderTargetFlag = texture.IsRenderTarget() ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
	const D3D12_RESOURCE_FLAGS depthStencilFlag = IsDepthFormat(texture.GetFormat()) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;
	CHECK((renderTargetFlag & depthStencilFlag) == 0);

	constexpr D3D12_HEAP_PROPERTIES heapProperties =
	{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 0,
		.VisibleNodeMask = 0,
	};
	const D3D12_RESOURCE_DESC1 textureDescription =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = texture.GetWidth(),
		.Height = texture.GetHeight(),
		.DepthOrArraySize = static_cast<uint16>(texture.GetCount()),
		.MipLevels = 1,
		.Format = ToD3D12(texture.GetFormat()),
		.SampleDesc = DefaultSampleDescription,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = renderTargetFlag | depthStencilFlag,
		.SamplerFeedbackMipRegion = {},
	};
	const D3D12_CLEAR_VALUE depthClear =
	{
		.Format = ToD3D12(texture.GetFormat()),
		.DepthStencil =
		{
			.Depth = D3D12_MAX_DEPTH,
			.Stencil = 0,
		},
	};

	static constexpr const DXGI_FORMAT* noCastableFormats = nullptr;
	TextureResource resource = nullptr;
	CHECK_RESULT(device->CreateCommittedResource3(&heapProperties, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &textureDescription, ToD3D12(initialLayout),
												  IsDepthFormat(texture.GetFormat()) ? &depthClear : nullptr, nullptr, 0, noCastableFormats, IID_PPV_ARGS(&resource)));
#if DEBUG
	CHECK_RESULT(resource->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.GetLength()), name.GetData()));
#else
	(void)name;
#endif
	return resource;
}

void WriteTexture(ID3D12Device11* device, const Texture& texture, const void* data, Array<UploadPair<Texture>>* pendingTextureUploads)
{
	CHECK(texture.GetType() == TextureType::Rectangle);

	const D3D12_RESOURCE_DESC1 textureDescription =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = texture.GetWidth(),
		.Height = texture.GetHeight(),
		.DepthOrArraySize = static_cast<uint16>(texture.GetCount()),
		.MipLevels = 1,
		.Format = ToD3D12(texture.GetFormat()),
		.SampleDesc = DefaultSampleDescription,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
		.SamplerFeedbackMipRegion = {},
	};
	static constexpr uint32 singleResource = 1;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
	uint32 rowCount;
	uint64 rowSize;
	uint64 totalSize;
	device->GetCopyableFootprints1(&textureDescription, 0, singleResource, 0, &layout, &rowCount, &rowSize, &totalSize);

	const BufferResource resource = AllocateBuffer(device, totalSize, true, "Upload [Texture]"_view);

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, nullptr, &mapped));
	for (usize row = 0; row < rowCount; ++row)
	{
		Platform::MemoryCopy
		(
			static_cast<uint8*>(mapped) + row * layout.Footprint.RowPitch,
			static_cast<const uint8*>(data) + row * rowSize,
			rowSize
		);
	}
	static constexpr const D3D12_RANGE* writeEverything = nullptr;
	resource->Unmap(0, writeEverything);

	pendingTextureUploads->Add({ resource, texture });
}

void WriteCubemapTexture(ID3D12Device11* device, const Texture& texture, const Array<uint8*>& faces, Array<UploadPair<Texture>>* pendingTextureUploads)
{
	CHECK(texture.GetType() == TextureType::Cubemap);
	CHECK(faces.GetLength() == 6);

	usize totalSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[6];
	usize subresourceSizes[6];
	uint32 rowCounts[6];
	usize rowSizes[6];

	for (usize subresourceIndex = 0; subresourceIndex < texture.GetCount(); ++subresourceIndex)
	{
		const D3D12_RESOURCE_DESC1 textureDescription =
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.Width = texture.GetWidth(),
			.Height = texture.GetHeight(),
			.DepthOrArraySize = static_cast<uint16>(texture.GetCount()),
			.MipLevels = 1,
			.Format = ToD3D12(texture.GetFormat()),
			.SampleDesc = DefaultSampleDescription,
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = D3D12_RESOURCE_FLAG_NONE,
			.SamplerFeedbackMipRegion = {},
		};
		static constexpr uint32 singleResource = 1;
		device->GetCopyableFootprints1(&textureDescription, static_cast<uint32>(subresourceIndex), singleResource, 0,
									   &layouts[subresourceIndex], &rowCounts[subresourceIndex], &rowSizes[subresourceIndex], &subresourceSizes[subresourceIndex]);

		totalSize += subresourceSizes[subresourceIndex];
	}

	const BufferResource resource = AllocateBuffer(device, totalSize, true, "Upload [Cubemap Texture]"_view);

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, nullptr, &mapped));
	for (usize subresourceIndex = 0; subresourceIndex < texture.GetCount(); ++subresourceIndex)
	{
		for (usize row = 0; row < rowCounts[subresourceIndex]; ++row)
		{
			Platform::MemoryCopy
			(
				static_cast<uint8*>(mapped) + subresourceIndex * subresourceSizes[subresourceIndex] + row * layouts[subresourceIndex].Footprint.RowPitch,
				static_cast<const uint8*>(faces[subresourceIndex]) + row * rowSizes[subresourceIndex],
				rowSizes[subresourceIndex]
			);
		}
	}
	static constexpr const D3D12_RANGE* writeEverything = nullptr;
	resource->Unmap(0, writeEverything);

	pendingTextureUploads->Add({ resource, texture });
}

D3D12Texture::D3D12Texture(ID3D12Device11* device, const Texture& texture, BarrierLayout initialLayout, TextureResource existingResource, StringView name)
	: Resource(existingResource ? existingResource : AllocateTexture(device, texture, initialLayout, name))
	, HeapIndices()
{
}
