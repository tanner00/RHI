#pragma once

#include "Common.hpp"

#include "Luft/Array.hpp"
#include "Luft/Hash.hpp"

enum class TextureType
{
	Rectangle,
	Cubemap,
};

enum class TextureFormat
{
	None,

	Rgba8Unorm,
	Rgba8SrgbUnorm,

	Rgba32Float,

	Bc7Unorm,
	Bc7SrgbUnorm,

	Depth32,
	Depth24Stencil8,
};

struct TextureDescription
{
	uint32 Width;
	uint32 Height;
	TextureType Type;
	TextureFormat Format;
	uint32 MipMapCount;
	bool RenderTarget;
	bool Storage;
};

class Texture final : public RhiHandle<Texture>
{
public:
	Texture()
		: RhiHandle(0)
		, Description()
	{
	}

	Texture(const RhiHandle& handle, TextureDescription&& Description)
		: RhiHandle(handle)
		, Description(Move(Description))
	{
	}

	uint32 GetWidth() const { return Description.Width; }
	uint32 GetHeight() const { return Description.Height; }
	TextureFormat GetFormat() const { return Description.Format; }
	TextureType GetType() const { return Description.Type; }

	bool IsRenderTarget() const { return Description.RenderTarget; }
	bool IsStorage() const { return Description.Storage; }

	uint32 GetMipMapCount() const { return Description.MipMapCount; }
	uint32 GetArrayCount() const { return Description.Type == TextureType::Cubemap ? 6 : 1; }
	uint32 GetCount() const { return GetArrayCount() * GetMipMapCount(); }

private:
	TextureDescription Description;
};

HASH_RHI_HANDLE(Texture);

class D3D12Texture
{
public:
	D3D12Texture(ID3D12Device11* device,
				 ViewHeap* constantBufferShaderResourceUnorderedAccessViewHeap,
				 ViewHeap* renderTargetViewHeap,
				 ViewHeap* depthStencilViewHeap,
				 const Texture& texture,
				 BarrierLayout initialLayout,
				 TextureResource existingResource,
				 StringView name);

	TextureResource GetTextureResource() const { CHECK(Resource); return Resource; }

	uint32 GetHeapIndex() const { CHECK(HeapIndex); return HeapIndex; }

	static void Write(ID3D12Device11* device, const Texture& texture, const void* data, Array<UploadPair<Texture>>* pendingTextureUploads);

	TextureResource Resource;
	uint32 HeapIndex;
};

inline bool IsDepthFormat(TextureFormat format)
{
	return format == TextureFormat::Depth32 || format == TextureFormat::Depth24Stencil8;
}

inline bool IsStencilFormat(TextureFormat format)
{
	return format == TextureFormat::Depth24Stencil8;
}
