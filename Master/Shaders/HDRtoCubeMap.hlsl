#ifndef HDRTOCUBEMAP_HLSL
#define HDRTOCUBEMAP_HLSL

#include "IBLConstants.hlsl"

// 球面マップからUV座標をサンプリングする関数
float2 SampleSphericalMap(float3 v)
{
    const float2 invAtan = float2(0.1591f, 0.3183f);
    
    float2 UV = float2(atan2(v.z, v.x), asin(v.y));
    UV *= invAtan;
    UV += 0.5f;
    
    UV = saturate(UV);
    UV.y = 1.0f - UV.y;
    
    return UV;
}

// HDR環境マップ（エクイレクタングラー）からキューブマップへ変換する
float4 HDRtoCubeMapPS(VertexPosHL pIn):SV_Target
{
    float2 uv = SampleSphericalMap(normalize(pIn.posL));
    
    float3 litColor = g_EquirectangularMap.SampleLevel(g_SamLinearClamp, uv, 0.0f).rgb;
    
    return float4(litColor, 1.0f);

}

#endif