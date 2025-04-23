#pragma once

#include "Barrier.hpp"
#include "Forward.hpp"

#include "Luft/FlagsEnum.hpp"
#include "Luft/String.hpp"

namespace RHI
{

enum class ResourceType : uint8
{
	Buffer,
	Texture2D,
};

enum class ResourceFormat : uint8
{
	None,

	Rgba8Unorm,
	Rgba8SrgbUnorm,

	Rgba16Float,
	Rgba32Float,

	Bc7Unorm,
	Bc7SrgbUnorm,

	Depth32,
	Depth24Stencil8,
};

enum class ResourceFlags : uint8
{
	None			= 0b000000,
	Upload			= 0b000001,
	Readback        = 0b000010,
	UnorderedAccess	= 0b000100,
	RenderTarget	= 0b001000,
	DepthStencil	= 0b010000,
	SwapChain       = 0b100000,
};
FLAGS_ENUM(ResourceFlags);

struct ResourceDimensions
{
	uint32 Width;
	uint32 Height;
};

struct ResourceDescription
{
	ResourceType Type;
	ResourceFormat Format;

	ResourceFlags Flags;
	BarrierLayout InitialLayout;

	union
	{
		usize Size;
		ResourceDimensions Dimensions;
	};
	uint16 MipMapCount;

	uint8 SwapChainIndex;

	StringView Name;
};

class Resource final : public ResourceDescription
{
public:
	Resource()
		: ResourceDescription()
		, Backend(nullptr)
	{
	}

	Resource(const ResourceDescription& description, RHI_BACKEND(Resource)* backend)
		: ResourceDescription(description)
		, Backend(backend)
	{
	}

	static Resource Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(Resource)* Backend;
};

inline bool IsDepthFormat(ResourceFormat format)
{
	return format == ResourceFormat::Depth32 || format == ResourceFormat::Depth24Stencil8;
}

inline bool IsStencilFormat(ResourceFormat format)
{
	return format == ResourceFormat::Depth24Stencil8;
}

}
