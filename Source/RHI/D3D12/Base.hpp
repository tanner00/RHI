#pragma once

#include "Luft/Base.hpp"

#include "D3D12/d3d12.h"

#define SAFE_RELEASE(p) if ((p)) { (p)->Release(); (p) = nullptr; }

#if DEBUG
#define CHECK_RESULT(expression) do { const HRESULT result##__LINE__ = (expression); CHECK(SUCCEEDED(result##__LINE__)); } while (false)
#else
#define CHECK_RESULT(expression) do { (expression); } while (false)
#endif

#if DEBUG
#define SET_D3D_NAME(resource, name) CHECK_RESULT((resource)->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<uint32>((name).GetLength()), (name).GetData()))
#else
#define SET_D3D_NAME(resource, name) (void)(name)
#endif

namespace RHI::D3D12
{

inline constexpr DXGI_SAMPLE_DESC DefaultSampleDescription = { 1, 0 };

inline constexpr usize MaxRenderTargetCount = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;

inline constexpr D3D12_RANGE ReadNothing = { 0, 0 };
inline constexpr D3D12_RANGE WriteNothing = { 0, 0 };
inline constexpr const D3D12_RANGE* WriteEverything = nullptr;

inline constexpr const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* NoPostBuildSizes = nullptr;

template<typename S, typename D>
struct UploadPair
{
	S Source;
	D Destination;
};

}
