#ifndef FRAMEBUFFER_FLAT_HLSL
#define FRAMEBUFFER_FLAT_HLSL

#include "PBRConstants.hlsl"

// R16G16B16A16_UNORM -> float4
float4 UnpackRGBA16(uint2 e)
{
    return float4(f16tof32(e), f16tof32(e >> 16));
}

// float4 -> R16G16B16A16_UNORM
uint2 PackRGBA16(float4 c)
{
    return f32tof16(c.rg) | (f32tof16(c.ba) << 16);
}

// 指定された2Dアドレスとサンプルインデックスに基づいて、1Dフレームバッファ配列内の位置を特定する
uint GetFramebufferSampleAddress(uint2 coords, uint sampleIndex)
{
    // 行優先: Row (x), Col (y), MSAA sample
    return (sampleIndex * g_FramebufferDimensions.y + coords.y) * g_FramebufferDimensions.x + coords.x;
}

#endif