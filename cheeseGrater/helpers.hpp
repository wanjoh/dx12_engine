#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>  // HRESULT
#include <exception>

// stolen from DXSampleHelper.h https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}