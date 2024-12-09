#pragma once

#include "Texture.hpp"

#include "D3D12/d3d12.h"

#define SAFE_RELEASE(p) if ((p)) { (p)->Release(); (p) = nullptr; }

#if DEBUG
#define CHECK_RESULT(expression) do { const HRESULT result = (expression); CHECK(SUCCEEDED(result)); } while (false)
#else
#define CHECK_RESULT(expression) do { (expression); } while (false)
#endif

#if DEBUG
#define SET_D3D_NAME(resource, name) CHECK_RESULT((resource)->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<uint32>((name).GetLength()), (name).GetData()))
#else
#define SET_D3D_NAME(resource, name) (void)(name)
#endif

inline constexpr DXGI_SAMPLE_DESC DefaultSampleDescription = { 1, 0 };

inline constexpr const DXGI_FORMAT* NoCastableFormats = nullptr;

inline constexpr D3D12_RANGE ReadNothing = { 0, 0 };
inline constexpr D3D12_RANGE WriteNothing = { 0, 0 };
inline constexpr const D3D12_RANGE* WriteEverything = nullptr;

inline constexpr D3D12_HEAP_PROPERTIES UploadHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_UPLOAD,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

inline constexpr D3D12_HEAP_PROPERTIES DefaultHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_DEFAULT,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

inline constexpr D3D12_HEAP_PROPERTIES ReadbackHeapProperties =
{
	.Type = D3D12_HEAP_TYPE_READBACK,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

inline D3D12_BARRIER_SYNC ToD3D12(BarrierStage b)
{
	return static_cast<D3D12_BARRIER_SYNC>(b);
}

inline D3D12_BARRIER_ACCESS ToD3D12(BarrierAccess b)
{
	return static_cast<D3D12_BARRIER_ACCESS>(b);
}

inline D3D12_BARRIER_LAYOUT ToD3D12(BarrierLayout b)
{
	return static_cast<D3D12_BARRIER_LAYOUT>(b);
};

inline DXGI_FORMAT ToD3D12(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::None:
		return DXGI_FORMAT_UNKNOWN;
	case TextureFormat::Rgba8:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::Rgba8Srgb:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case TextureFormat::Bc7:
		return DXGI_FORMAT_BC7_UNORM;
	case TextureFormat::Bc7Srgb:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	case TextureFormat::Depth24Stencil8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case TextureFormat::Depth32:
		return DXGI_FORMAT_D32_FLOAT;
	}
	CHECK(false);
	return DXGI_FORMAT_UNKNOWN;
}

inline DXGI_FORMAT ToD3D12View(TextureFormat format, ViewType type)
{
	switch (format)
	{
	case TextureFormat::Depth24Stencil8:
		if (type == ViewType::ShaderResource)
		{
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
		break;
	case TextureFormat::Depth32:
		if (type == ViewType::ShaderResource)
		{
			return DXGI_FORMAT_R32_TYPELESS;
		}
		break;
	}
	return ToD3D12(format);
}

