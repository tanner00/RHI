#pragma once

#include "Common.hpp"

#include "D3D12/dxgiformat.h"

enum class TextureType
{
	Rectangle,
	Cubemap,
};

enum class TextureFormat
{
	None,

	Rgba8,
	Rgba8Srgb,

	Bc7,
	Bc7Srgb,

	Depth32,
	Depth24Stencil8,
};

struct TextureDescription
{
	uint32 Width;
	uint32 Height;
	TextureType Type;
	TextureFormat Format;
	bool RenderTarget;
};

class Texture final : public RHI_HANDLE(Texture)
{
public:
	RHI_HANDLE_BODY(Texture);

	uint32 GetWidth() const { return Description.Width; }
	uint32 GetHeight() const { return Description.Height; }
	TextureFormat GetFormat() const { return Description.Format; }
	TextureType GetType() const { return Description.Type; }

	bool IsRenderTarget() const { return Description.RenderTarget; }

	usize GetCount() const { return Description.Type == TextureType::Cubemap ? 6 : 1; }

private:
	TextureDescription Description;
};

template<>
struct Hash<Texture>
{
	uint64 operator()(const Texture& texture) const
	{
		const usize handle = texture.Get();
		return HashFnv1a(&handle, sizeof(handle));
	}
};

inline bool operator==(const Texture& a, const Texture& b)
{
	return a.Get() == b.Get();
}

class D3D12Texture
{
public:
	TextureResource GetTextureResource() const { CHECK(Resource); return Resource; }

	TextureResource Resource;
	usize HeapIndices[static_cast<usize>(ViewType::Count)];
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

inline bool IsDepthFormat(TextureFormat format)
{
	return format == TextureFormat::Depth32 || format == TextureFormat::Depth24Stencil8;
}

inline bool IsStencilFormat(TextureFormat format)
{
	return format == TextureFormat::Depth24Stencil8;
}

TextureResource AllocateTexture(ID3D12Device11* device, const Texture& texture, BarrierLayout initialLayout, StringView name);

D3D12Texture CreateD3D12Texture(ID3D12Device11* device, const Texture& texture, BarrierLayout initialLayout, StringView name, TextureResource existingResource);
