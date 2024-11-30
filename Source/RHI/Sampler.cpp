#include "Sampler.hpp"
#include "ViewHeap.hpp"

#include "D3D12/d3d12.h"

static D3D12_TEXTURE_ADDRESS_MODE ToD3D12(SamplerAddress address)
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

static D3D12_FILTER ToD3D12(SamplerFilter minification, SamplerFilter magnification)
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

D3D12Sampler::D3D12Sampler(ID3D12Device11* device, const Sampler& sampler, ViewHeap* samplerViewHeap)
	: HeapIndex(samplerViewHeap->AllocateIndex())
{
	const D3D12_SAMPLER_DESC2 samplerDescription =
	{
		.Filter = ToD3D12(sampler.GetMinificationFilter(), sampler.GetMagnificationFilter()),
		.AddressU = ToD3D12(sampler.GetHorizontalAddress()),
		.AddressV = ToD3D12(sampler.GetVerticalAddress()),
		.AddressW = ToD3D12(SamplerAddress::Wrap),
		.MipLODBias = 0,
		.MaxAnisotropy = D3D12_DEFAULT_MAX_ANISOTROPY,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
		.FloatBorderColor =
		{
			sampler.GetBorderColor().X,
			sampler.GetBorderColor().Y,
			sampler.GetBorderColor().Z,
			sampler.GetBorderColor().W,
		},
		.MinLOD = 0.0f,
		.MaxLOD = D3D12_FLOAT32_MAX,
		.Flags = D3D12_SAMPLER_FLAG_NONE,
	};
	device->CreateSampler2(&samplerDescription, D3D12_CPU_DESCRIPTOR_HANDLE { samplerViewHeap->GetCpu(HeapIndex) });
}
