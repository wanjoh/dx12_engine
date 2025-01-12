#pragma once

// minimizes the number of header files that are included in Windows.h
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <shellapi.h>  // CommandLineToArgW

// min/max macros conflict; only use std::min and std::max from <algorithm>
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include <wrl.h>  // Microsoft::WRL::ComPtr<>

#include <d3dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

constexpr uint32_t DEFAULT_WINDOW_WIDTH  = 1280;
constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 720;

// stolen from DXSampleHelper.h https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}