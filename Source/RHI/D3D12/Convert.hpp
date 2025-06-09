#pragma once

#include "AccelerationStructure.hpp"
#include "BufferView.hpp"
#include "Resource.hpp"
#include "Sampler.hpp"
#include "TextureView.hpp"

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
	{
		nativeFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	if (HasFlags(flags, ResourceFlags::RenderTarget))
	{
		nativeFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	if (HasFlags(flags, ResourceFlags::DepthStencil))
	{
		nativeFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if (HasFlags(flags, ResourceFlags::AccelerationStructure))
	{
		nativeFlags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
	}
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
	case ResourceType::AccelerationStructureInstances:
		return D3D12_RESOURCE_DIMENSION_BUFFER;
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
	case ResourceFormat::RGBA8UNorm:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case ResourceFormat::RGBA8UNormSRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case ResourceFormat::RGBA16Float:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case ResourceFormat::RGBA32Float:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case ResourceFormat::RG32UInt:
		return DXGI_FORMAT_R32G32_UINT;
	case ResourceFormat::Depth24Stencil8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case ResourceFormat::Depth32:
		return DXGI_FORMAT_D32_FLOAT;
	case ResourceFormat::BC1UNorm:
		return DXGI_FORMAT_BC1_UNORM;
	case ResourceFormat::BC3UNorm:
		return DXGI_FORMAT_BC3_UNORM;
	case ResourceFormat::BC5UNorm:
		return DXGI_FORMAT_BC5_UNORM;
	case ResourceFormat::BC7UNorm:
		return DXGI_FORMAT_BC7_UNORM;
	case ResourceFormat::BC7UNormSRGB:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
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

inline D3D12_RAYTRACING_GEOMETRY_DESC To(const AccelerationStructureGeometry& geometry)
{
	CHECK(geometry.VertexBuffer.Stride == sizeof(float[3]));
	CHECK(geometry.IndexBuffer.Stride == sizeof(uint16) || geometry.IndexBuffer.Stride == sizeof(uint32));
	CHECK(geometry.VertexBuffer.Size % geometry.VertexBuffer.Stride == 0 && geometry.IndexBuffer.Size % geometry.IndexBuffer.Stride == 0);

	return D3D12_RAYTRACING_GEOMETRY_DESC
	{
		.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
		.Flags = geometry.Translucent ? D3D12_RAYTRACING_GEOMETRY_FLAG_NONE : D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
		.Triangles = D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC
		{
			.Transform3x4 = D3D12_GPU_VIRTUAL_ADDRESS { 0 },
			.IndexFormat = geometry.IndexBuffer.Stride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
			.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
			.IndexCount = static_cast<uint32>(geometry.IndexBuffer.Size / geometry.IndexBuffer.Stride),
			.VertexCount = static_cast<uint32>(geometry.VertexBuffer.Size / geometry.VertexBuffer.Stride),
			.IndexBuffer = geometry.IndexBuffer.Resource.Backend->Native->GetGPUVirtualAddress() + geometry.IndexBuffer.Offset,
			.VertexBuffer = D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE
			{
				.StartAddress = geometry.VertexBuffer.Resource.Backend->Native->GetGPUVirtualAddress() + geometry.VertexBuffer.Offset,
				.StrideInBytes = geometry.VertexBuffer.Stride,
			},
		},
	};
}

inline D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS To(const D3D12_RAYTRACING_GEOMETRY_DESC& description)
{
	return D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
	{
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
		.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
		.NumDescs = 1,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = &description,
	};
}

inline D3D12_RAYTRACING_INSTANCE_DESC To(const AccelerationStructureInstance& instance)
{
	CHECK(instance.AccelerationStructureResource.IsValid());
	CHECK(instance.ID < 0xFFFFFF);

	const Matrix& transform = instance.Transform;

	return D3D12_RAYTRACING_INSTANCE_DESC
	{
		.Transform =
		{
			{ transform.M00, transform.M01, transform.M02, transform.M03 },
			{ transform.M10, transform.M11, transform.M12, transform.M13 },
			{ transform.M20, transform.M21, transform.M22, transform.M23 },
		},
		.InstanceID = instance.ID,
		.InstanceMask = 0xFF,
		.InstanceContributionToHitGroupIndex = 0,
		.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE,
		.AccelerationStructure = instance.AccelerationStructureResource.Backend->Native->GetGPUVirtualAddress(),
	};
}

inline D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS To(const Buffer& instances)
{
	CHECK(instances.Stride == sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	return D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
	{
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
		.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
		.NumDescs = static_cast<uint32>(instances.Size / sizeof(D3D12_RAYTRACING_INSTANCE_DESC)),
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.InstanceDescs = instances.Resource.Backend->Native->GetGPUVirtualAddress(),
	};
}

inline ToViewType<ViewType::ShaderResource>::Type To(const AccelerationStructureDescription& description)
{
	return
	{
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.RaytracingAccelerationStructure = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV
		{
			.Location = description.AccelerationStructureResource.Backend->Native->GetGPUVirtualAddress(),
		},
	};
}

inline ToViewType<ViewType::ConstantBuffer>::Type To(const BufferViewDescription& description, const Resource* resource)
{
	return
	{
		.BufferLocation = resource->Native->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<uint32>(description.Buffer.Size),
	};
}

template<ViewType V>
typename ToViewType<V>::Type To(const BufferViewDescription& description);

template<>
inline ToViewType<ViewType::ShaderResource>::Type To<ViewType::ShaderResource>(const BufferViewDescription& description)
{
	const bool byteAddressBuffer = description.Buffer.Stride == 0;

	const DXGI_FORMAT format = byteAddressBuffer ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
	const usize count = byteAddressBuffer ? (description.Buffer.Size / 4) : (description.Buffer.Size / description.Buffer.Stride);
	const D3D12_BUFFER_SRV_FLAGS flags = byteAddressBuffer ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

	return
	{
		.Format = format,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = D3D12_BUFFER_SRV
		{
			.FirstElement = 0,
			.NumElements = static_cast<uint32>(count),
			.StructureByteStride = static_cast<uint32>(description.Buffer.Stride),
			.Flags = flags,
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
			.NumElements = static_cast<uint32>(description.Buffer.Size / sizeof(uint32)),
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
		.Format = To(description.Resource.Format),
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = D3D12_TEX2D_SRV
		{
			.MostDetailedMip = 0,
			.MipLevels = description.Resource.MipMapCount != 0 ? description.Resource.MipMapCount : static_cast<uint16>(1),
			.PlaneSlice = 0,
			.ResourceMinLODClamp = 0,
		},
	};
}

template<>
inline ToViewType<ViewType::UnorderedAccess>::Type To<ViewType::UnorderedAccess>(const TextureViewDescription& description)
{
	return
	{
		.Format = To(description.Resource.Format),
		.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
		.Texture2D = D3D12_TEX2D_UAV
		{
			.MipSlice = 0,
			.PlaneSlice = 0,
		},
	};
}

template<>
inline ToViewType<ViewType::RenderTarget>::Type To<ViewType::RenderTarget>(const TextureViewDescription& description)
{
	return
	{
		.Format = To(description.Resource.Format),
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
		.Format = To(description.Resource.Format),
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
		.FloatBorderColor = { description.BorderColor.R, description.BorderColor.G, description.BorderColor.B, description.BorderColor.A },
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
