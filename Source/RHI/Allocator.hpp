#pragma once

#include "Luft/Allocator.hpp"

namespace RHI
{
inline GlobalAllocator* const Allocator = &GlobalAllocator::Get();
}
