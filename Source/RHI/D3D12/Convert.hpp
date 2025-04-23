#pragma once

#include "Resource.hpp"

#include "RHI/View.hpp"

namespace RHI::D3D12
{

template<ViewType V>
struct ToViewType;

template<> struct ToViewType<ViewType::ConstantBuffer>  { using Type = D3D12_CONSTANT_BUFFER_VIEW_DESC; };
template<> struct ToViewType<ViewType::ShaderResource>  { using Type = D3D12_SHADER_RESOURCE_VIEW_DESC; };
template<> struct ToViewType<ViewType::UnorderedAccess> { using Type = D3D12_UNORDERED_ACCESS_VIEW_DESC; };
template<> struct ToViewType<ViewType::Sampler>         { using Type = D3D12_SAMPLER_DESC2; };
template<> struct ToViewType<ViewType::RenderTarget>    { using Type = D3D12_RENDER_TARGET_VIEW_DESC; };
template<> struct ToViewType<ViewType::DepthStencil>    { using Type = D3D12_DEPTH_STENCIL_VIEW_DESC; };

inline D3D12_BARRIER_SYNC To(BarrierStage stage)
{
	return static_cast<D3D12_BARRIER_SYNC>(stage);
}

inline D3D12_BARRIER_ACCESS To(BarrierAccess access)
{
	return static_cast<D3D12_BARRIER_ACCESS>(access);
}

inline D3D12_BARRIER_LAYOUT To(BarrierLayout layout)
{
	return static_cast<D3D12_BARRIER_LAYOUT>(layout);
}

inline D3D12_RESOURCE_FLAGS To(ResourceFlags flags)
{
	D3D12_RESOURCE_FLAGS nativeFlags = D3D12_RESOURCE_FLAG_NONE;
	if (HasFlags(flags, ResourceFlags::UnorderedAccess))
		nativeFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (HasFlags(flags, ResourceFlags::RenderTarget))
		nativeFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (HasFlags(flags, ResourceFlags::DepthStencil))
		nativeFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	return nativeFlags;
}

inline D3D12_RESOURCE_DIMENSION To(ResourceType type)
{
	switch (type)
	{
	case ResourceType::Buffer:
		return D3D12_RESOURCE_DIMENSION_BUFFER;
	case ResourceType::Texture2D:
		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	}
	CHECK(false);
	return D3D12_RESOURCE_DIMENSION_UNKNOWN;
}

inline DXGI_FORMAT To(ResourceFormat format)
{
	switch (format)
	{
	case ResourceFormat::None:
		return DXGI_FORMAT_UNKNOWN;
	case ResourceFormat::Rgba8Unorm:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case ResourceFormat::Rgba8SrgbUnorm:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case ResourceFormat::Rgba32Float:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case ResourceFormat::Rgba16Float:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case ResourceFormat::Bc7Unorm:
		return DXGI_FORMAT_BC7_UNORM;
	case ResourceFormat::Bc7SrgbUnorm:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	case ResourceFormat::Depth24Stencil8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case ResourceFormat::Depth32:
		return DXGI_FORMAT_D32_FLOAT;
	}
	CHECK(false);
	return DXGI_FORMAT_UNKNOWN;
}

static D3D12_TEXTURE_ADDRESS_MODE To(SamplerAddress address)
{
	switch (address)
	{
	case SamplerAddress::Wrap:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case SamplerAddress::Mirror:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SamplerAddress::Clamp:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SamplerAddress::Border:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	}
	CHECK(false);
	return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

static D3D12_FILTER To(SamplerFilter minification, SamplerFilter magnification)
{
	switch (minification)
	{
	case SamplerFilter::Point:
		switch (magnification)
		{
		case SamplerFilter::Point:
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		case SamplerFilter::Linear:
			return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		default:
			CHECK(false);
		}
		break;
	case SamplerFilter::Linear:
		switch (magnification)
		{
		case SamplerFilter::Point:
			return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		case SamplerFilter::Linear:
			return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		default:
			break;
		}
		break;
	case SamplerFilter::Anisotropic:
		switch (magnification)
		{
		case SamplerFilter::Anisotropic:
			return D3D12_FILTER_ANISOTROPIC;
		default:
			break;
		}
		break;
	}
	CHECK(false);
	return D3D12_FILTER_MIN_MAG_MIP_POINT;
}

inline D3D12_RESOURCE_DESC1 To(const ResourceDescription& description)
{
	return D3D12_RESOURCE_DESC1
	{
		.Dimension = To(description.Type),
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = description.Format == ResourceFormat::None ? description.Size : description.Dimensions.Width,
		.Height = description.Format == ResourceFormat::None ? 1 : description.Dimensions.Height,
		.DepthOrArraySize = 1,
		.MipLevels = description.MipMapCount != 0 ? description.MipMapCount : static_cast<uint16>(1),
		.Format = To(description.Format),
		.SampleDesc = DefaultSampleDescription,
		.Layout = description.Format == ResourceFormat::None ? D3D12_TEXTURE_LAYOUT_ROW_MAJOR : D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = To(description.Flags),
		.SamplerFeedbackMipRegion = {},
	};
}

inline ToViewType<ViewType::ConstantBuffer>::Type To(const BufferViewDescription& description, const Resource* resource)
{
	return
	{
		.BufferLocation = resource->Native->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<uint32>(description.Size),
	};
}

template<ViewType V>
typename ToViewType<V>::Type To(const BufferViewDescription& description);

template<>
inline ToViewType<ViewType::ShaderResource>::Type To<ViewType::ShaderResource>(const BufferViewDescription& description)
{
	return
	{
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = D3D12_BUFFER_SRV
		{
			.FirstElement = 0,
			.NumElements = description.Stride ? static_cast<uint32>(description.Size / description.Stride) : 1,
			.StructureByteStride = static_cast<uint32>(description.Stride ? description.Stride : description.Size),
			.Flags = D3D12_BUFFER_SRV_FLAG_NONE,
		},
	};
}

template<>
inline ToViewType<ViewType::UnorderedAccess>::Type To<ViewType::UnorderedAccess>(const BufferViewDescription& description)
{
	return
	{
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer =
		{
			.FirstElement = 0,
			.NumElements = static_cast<uint32>(description.Size / sizeof(uint32)),
			.StructureByteStride = 0,
			.CounterOffsetInBytes = 0,
			.Flags = D3D12_BUFFER_UAV_FLAG_RAW,
		},
	};
}

template<ViewType V>
typename ToViewType<V>::Type To(const TextureViewDescription& description);

template<>
inline ToViewType<ViewType::ShaderResource>::Type To<ViewType::ShaderResource>(const TextureViewDescription& description)
{
	return
	{
		.Format = To(description.Format),
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = D3D12_TEX2D_SRV
		{
			.MostDetailedMip = 0,
			.MipLevels = description.MipMapCount != 0 ? description.MipMapCount : static_cast<uint16>(1),
			.PlaneSlice = 0,
			.ResourceMinLODClamp = 0,
		},
	};
}

template<>
inline ToViewType<ViewType::RenderTarget>::Type To<ViewType::RenderTarget>(const TextureViewDescription& description)
{
	return
	{
		.Format = To(description.Format),
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D = D3D12_TEX2D_RTV
		{
			.MipSlice = 0,
			.PlaneSlice = 0,
		},
	};
}

template<>
inline ToViewType<ViewType::DepthStencil>::Type To<ViewType::DepthStencil>(const TextureViewDescription& description)
{
	return
	{
		.Format = To(description.Format),
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Flags = D3D12_DSV_FLAG_NONE,
		.Texture2D =
		{
			.MipSlice = 0,
		},
	};
}

inline ToViewType<ViewType::Sampler>::Type To(const SamplerDescription& description)
{
	return
	{
		.Filter = To(description.MinificationFilter, description.MagnificationFilter),
		.AddressU = To(description.HorizontalAddress),
		.AddressV = To(description.VerticalAddress),
		.AddressW = To(SamplerAddress::Wrap),
		.MipLODBias = 0,
		.MaxAnisotropy = D3D12_DEFAULT_MAX_ANISOTROPY,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
		.FloatBorderColor = { description.BorderColor.X, description.BorderColor.Y, description.BorderColor.Z, description.BorderColor.W, },
		.MinLOD = 0.0f,
		.MaxLOD = D3D12_FLOAT32_MAX,
		.Flags = D3D12_SAMPLER_FLAG_NONE,
	};
}

inline DXGI_FORMAT MaskToFormat(uint8 mask)
{
	switch (mask)
	{
	case 0b0001:
		return DXGI_FORMAT_R32_FLOAT;
	case 0b0011:
		return DXGI_FORMAT_R32G32_FLOAT;
	case 0b0111:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case 0b1111:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:
		break;
	}
	CHECK(false);
	return DXGI_FORMAT_UNKNOWN;
}

}
