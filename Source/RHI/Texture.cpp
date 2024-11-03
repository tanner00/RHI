#include "Texture.hpp"
#include "BarrierConversion.hpp"

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

D3D12Texture CreateD3D12Texture(ID3D12Device11* device, const Texture& texture, BarrierLayout initialLayout, StringView name, TextureResource existingResource)
{
	return D3D12Texture
	{
		.Resource = existingResource ? existingResource : AllocateTexture(device, texture, initialLayout, name),
		.HeapIndices = {},
	};
}
