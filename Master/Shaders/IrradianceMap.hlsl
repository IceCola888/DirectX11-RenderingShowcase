#ifndef IRRADIANCEMAP_HLSL
#define IRRADIANCEMAP_HLSL

#include "IBLConstants.hlsl"

float4 IrradianceCubeMapPS(VertexPosHL pIn) : SV_Target
{
    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    
    // 接空間を構築するための基底ベクトル
    float3 normal = normalize(pIn.posL);
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 right = cross(up, normal);
    up = cross(normal, right);
    
    // 積分サンプリングのステップサイズ
    float sampleDelta = 0.025f;
    float sampleCount = 0.0f;
    
    // 半球方向に対してサンプリング
    for (float phi = 0.0f; phi < 2.0f * X_PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * X_PI; theta += sampleDelta)
        {
            // 接空間での方向ベクトルを構築
            float3 tangentSample = float3(
                                        sin(theta) * cos(phi), 
                                        sin(theta) * sin(phi), 
                                        cos(theta));
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            
            // 環境マップをサンプリングして放射照度に加算
            irradiance += g_SkyboxTexture.SampleLevel(g_SamAnisotropicWarp, sampleVec, 0.0f).rgb * cos(theta) * sin(theta);
            
            sampleCount++;
        }
    }
    
    irradiance = X_PI * irradiance * (1.0f / sampleCount);
    
    return float4(irradiance, 1.0f);
    
}
#endif