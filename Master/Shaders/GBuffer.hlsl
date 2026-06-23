#ifndef GBUFFER_HLSL
#define GBUFFER_HLSL

#include "Rendering.hlsl"


//--------------------------------------------------------------------------------------
// G-buffer
//--------------------------------------------------------------------------------------

void GBufferPS(VertexPosHVNormalVTex input, uniform bool alphaClip, out GBuffer outputGbuffer)
{
    // ジオメトリ情報からサーフェスデータを計算
    SurfaceData surface = ComputeSurfaceDataFromGeometry(input);
    
    // アルファ値によるピクセル破棄
    if(alphaClip)
    {
        clip(surface.albedo.a - 0.05f);
    }
    
    // 法線を圧縮 + roughness + metallic
    outputGbuffer.normalRoughnessMetallic = float4(EncodeSphereMap(surface.normalV), surface.roughness, surface.metallic);
    outputGbuffer.albedo = surface.albedo;
    // 隣接ピクセルとの深度の変化率を計算
    outputGbuffer.posZGrad = float2(ddx_fine(surface.posV.z), ddy_fine(surface.posV.z));
}


//--------------------------------------------------------------------------------------
// Debug
//--------------------------------------------------------------------------------------

float4 DebugNormalPS(float4 posViewport : SV_Position) : SV_Target
{
    float4 normalRM = g_GBufferTextures[0].Load(posViewport.xy, 0).xyzw;
    float3 normalV = DecodeSphereMap(normalRM.xy);
    float3 normalW = mul(normalV, (float3x3) g_InvView).xyz;
    // [-1, 1] => [0, 1]
    return pow(float4((normalW + 1.0f) / 2.0f, 1.0f), 2.2);
}

float4 DebugRoughnessPS(float4 posViewport : SV_Position) : SV_Target
{
    float roughness = g_GBufferTextures[0].Load(posViewport.xy,0).b;
    return float4(roughness.rrr, 1.0f);
}

float4 DebugMetallicPS(float4 posViewport : SV_Position) : SV_Target
{
    float metallic = g_GBufferTextures[0].Load(posViewport.xy, 0).a;
    return float4(metallic.rrr, 1.0f);
}

float4 DebugAlbedoPS(float4 posViewport : SV_Position) : SV_Target
{
    float4 albedo = g_GBufferTextures[1].Load(posViewport.xy, 0).rgba;
    return albedo;
}

float4 DebugPosZGradPS(float4 posViewport : SV_Position) : SV_Target
{
    float2 posZGrad = g_GBufferTextures[2].Load(posViewport.xy, 0).xy;
    return pow(float4(posZGrad, 0.0f, 1.0f), 2.2f);
}

void ComputeSurfaceDataFromGBufferAllSamples(uint2 posViewport,
                                             out SurfaceData surface[MSAA_SAMPLES])
{
    // 各サンプルからデータを取得
    // 通常は一部のサンプルしか使用されないため、ループ展開により不要なコードの最適化を促進
    [unroll]
    for (uint i = 0; i < MSAA_SAMPLES; ++i)
    {
        surface[i] = ComputeSurfaceDataFromGBufferSample(posViewport, i);
    }
}

bool RequiresPerSampleShading(SurfaceData surface[MSAA_SAMPLES])
{
    bool perSample = false;
    
    // 隣接ピクセルとの深度勾配の変化量
    const float maxZDelta = abs(surface[0].posV_DX.z) + abs(surface[0].posV_DY.z);
    const float minNormalDot = 0.99f; // 法線の角度差が約8度以内であることを許容
    
    [unroll]
    for (uint i = 1; i < MSAA_SAMPLES; ++i)
    {
        // 同一ピクセル内の MSAA サンプル間での深度差が大きいかどうかを検出
        // この差が隣接ピクセルとの勾配を超える場合は、境界の可能性があるため、サンプル単位で処理が必要
        perSample = perSample ||
            abs(surface[i].posV.z - surface[0].posV.z) > maxZDelta;
        
        // 法線の角度差が大きい場合、異なる三角形やサーフェスの可能性がある
        perSample = perSample ||
                        dot(surface[i].normalV, surface[0].normalV) < minNormalDot;
    }
    
    return perSample;
}

// サンプル単位（1）/ ピクセル単位（0）のフラグを使って
// ステンシルマスク値を初期化する
void RequiresPerSampleShadingPS(float4 posViewport : SV_Position)
{
    // MSAAに必要な表面マテリアル情報を各サンプルから取得する
    SurfaceData surfaceSamples[MSAA_SAMPLES];
    ComputeSurfaceDataFromGBufferAllSamples(uint2(posViewport.xy), surfaceSamples);
    bool perSample = RequiresPerSampleShading(surfaceSamples);
    
    // サンプル単位のシェーディングが不要であれば
    // このピクセルフラグメントを破棄する（例：ステンシルに書き込まない）
    [flatten]
    if (!perSample)
    {
        // ピクセルを破棄する
        discard;
    }

}

#endif