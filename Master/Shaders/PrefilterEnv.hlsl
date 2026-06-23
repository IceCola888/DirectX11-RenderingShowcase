#ifndef PREFILTERENV_HLSL
#define PREFILTERENV_HLSL

#include "IBLConstants.hlsl"



float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = X_PI * denom * denom;
    
    return nom / denom;
}

// 低差異系列のハミング距離を用いた順序でビット反転を行う
float RadicalInverse_Vdc(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// ハマースレイサンプリング：0〜1の一様分布にサンプル点を配置
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_Vdc(i));
}

// GGX分布に基づく重要度サンプリング
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    // 方位角
    float phi = 2.0 * X_PI * Xi.x;
    // コサイン加重分布（GGXの特性）による仰角θの計算
    float cosTheta = saturate(sqrt((1.0f - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y)));
    float sinTheta = saturate(sqrt(1.0f - cosTheta * cosTheta));
    
    
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    // ワールド空間への変換（接ベクトル空間構築）
    float3 up = abs(N.z) < 0.999 ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    
    return normalize(sampleVec);
    
}

float4 PrefilterCubeMapPS(VertexPosHL pIn) : SV_Target
{
    float3 N = normalize(pIn.posL);
    float3 R = N;
    float3 V = R;
    
    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0f;
    float3 prefilterColor = float3(0.0f, 0.0f, 0.0f);
    
    for (uint i = 0u; i < SAMPLE_COUNT; i++)
    {
        // サンプリングポイント（低差異列）
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        // GGXに基づく重要度サンプリングでハーフベクトルを取得
        float3 H = ImportanceSampleGGX(Xi, N, g_roughness);
        // Hから入射方向Lを導出（反射ベクトル
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        // 法線方向との角度
        float NdotL = max(dot(N, L), 0.0f);
        
        if (NdotL > 0.0)
        {
            
            float D = DistributionGGX(N, H, g_roughness);
            float NdotH = max(dot(N, H), 0.0f);
            float HdotV = max(dot(H, V), 0.0f);
            float pdf = D * NdotH / (4.0f * HdotV) + 0.0001f;
            // 各サンプルの面積（立方体環境マップの分解能に基づく）
            float resolution = 1024.0f;
            float saTexel = 4.0f * X_PI / (6.0f * resolution * resolution);
            float saSample = 1.0f / (float(SAMPLE_COUNT) * pdf + 0.0001f);
            // Mipmapレベルの決定（粗さに応じてLODを調整）
            float mipLevel = (g_roughness == 0.0f ? 0.0 : 0.5f * log2(saSample / saTexel));
 
            prefilterColor += g_SkyboxTexture.SampleLevel(g_SamAnisotropicWarp, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilterColor = prefilterColor / totalWeight;
    
    return float4(prefilterColor, 1.0f);
}

#endif