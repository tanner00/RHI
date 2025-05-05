#pragma once

#include "Resource.hpp"

#include "Luft/Base.hpp"

namespace RHI
{

struct Buffer
{
	Resource Resource;
	usize Size;
	usize Stride;
};

struct SubBuffer
{
	Resource Resource;
	usize Size;
	usize Stride;
	usize Offset;
};

}
