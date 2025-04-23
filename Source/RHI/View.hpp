#pragma once

#include "Luft/Base.hpp"

namespace RHI
{

enum class ViewType : uint8
{
	ConstantBuffer,
	ShaderResource,
	UnorderedAccess,
	Sampler,
	RenderTarget,
	DepthStencil,
};

}
