#pragma once

#include "GraphicsContext.hpp"
#include "D3D12/d3d12.h"

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
