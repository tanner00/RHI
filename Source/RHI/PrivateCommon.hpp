#pragma once

#include "D3D12/d3d12.h"

#define SAFE_RELEASE(p) if ((p)) { (p)->Release(); (p) = nullptr; }

#if DEBUG
#define CHECK_RESULT(expression) do { const HRESULT result = (expression); CHECK(SUCCEEDED(result)); } while (false)
#else
#define CHECK_RESULT(expression) do { (expression); } while (false)
#endif

#if DEBUG
#define SetD3DName(resource, name) CHECK_RESULT((resource)->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<uint32>((name).GetLength()), (name).GetData()))
#else
#define SetD3DName(resource, name) (void)(name)
#endif

inline constexpr DXGI_SAMPLE_DESC DefaultSampleDescription = { 1, 0 };

inline constexpr D3D12_RANGE ReadNothing = { 0, 0 };
inline constexpr D3D12_RANGE WriteNothing = { 0, 0 };
inline constexpr const D3D12_RANGE* WriteEverything = nullptr;
