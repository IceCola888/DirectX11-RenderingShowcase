#ifndef IBLCONSTANTS_HLSL
#define IBLCONSTANTS_HLSL

#include "Constants.hlsl"

cbuffer CB : register(b0)
{
    matrix  g_WorldViewProj;
    float   g_roughness;
}

struct VertexPos
{
    float3 posL : POSITION;
};

struct VertexPosHL
{
    float4 posH : SV_POSITION;
    float3 posL : POSITION;
};

Texture2D<float4>   g_EquirectangularMap :  register(t0);
TextureCube<float4> g_SkyboxTexture :       register(t1);

SamplerState g_SamLinearClamp :     register(s0);
SamplerState g_SamAnisotropicWarp : register(s1);

// 環境キューブマップの通用VS
VertexPosHL EnvCubeMapVS(VertexPos vIn)
{
    VertexPosHL vOut;
    
    vOut.posH = mul(float4(vIn.posL, 0.0f), g_WorldViewProj).xyww;
    vOut.posL = vIn.posL;
    
    return vOut;
}

#endif