#ifndef PBRLIGHTING_HLSL
#define PBRLIGHTING_HLSL

#include "Constants.hlsl"


struct DirectionalLight
{
    float3 color;
    float  intensity;
    
    float3 direction;
    float  pad1;
};

struct PointLight
{
    float3 position;
    float  range;
    
    float3 color;
    float  intensity;
};

struct SpotLight
{
    float3 color;
    float  intensity;
    
    float3 position;
    float  range;
    
    float3 direction;
    float  spotRadius;
    
    float2 spotAngles;
    float2 pad2;
};

// スポットライトの減衰係数を計算（入射角度に基づく）
float SpotAttenuation(float3 lightDir,float3 spotDirection,float2 spotAngles)
{
    float coneAngleFalloff = Square(saturate((dot(-lightDir, spotDirection) - spotAngles.x) * spotAngles.y));
    
    return coneAngleFalloff;
}

// 点ライト・スポットライトの距離減衰を計算（距離とレンジに基づく）
float CalcDistanceAttenuation(float3 worldPos, float3 lightPos, float range)
{
    float3 worldLightVector = lightPos - worldPos;
    float distanceSqr = dot(worldLightVector, worldLightVector);
    
    float distanceAttenuation = 1 / (distanceSqr + 1);
    
    float rangeInv = rcp(max(range, 0.001f));
    
    float lightRadiusMask = Square(saturate(1 - Square(distanceSqr * rangeInv * rangeInv)));
    distanceAttenuation *= lightRadiusMask;
    
    return distanceAttenuation;
}

// F
// F0：基礎反射率（材質特性により決定、金属度など）
// VoH：視線ベクトル V とハーフベクトル H の内積
float3 FresnelSchlick(float3 F0, float VoH)
{
    return F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
}

// 分布関数を用いて法線分布項 Dを計算する
float GGX(float a2, float NoH)
{
    // NoH：法線 N とハーフベクトル H の内積
    float NoH2 = NoH * NoH;
    float d = NoH2 * (a2 - 1.0f) + 1.0f;
    
    return a2 / (X_PI * d * d);
}

// Schlick-GGXの幾何項（G1）を計算する関数。視線方向における遮蔽効果を近似する
float GeometrySchlickGGX(float roughness, float NoV)
{
    float k = pow(roughness + 1.0f, 2.0f) / 8.0f;

    return NoV / (NoV * (1 - k) + k);
    
}

// GGXスペキュラー反射モデル（Cook-Torrance BRDF の鏡面反射成分）
float3 SpecularGGX(float3 N, float3 L, float3 V, float roughness, float3 F0)
{
     // 視線ベクトルVと光ベクトルLの中間ベクトルHを計算
    float3 H = normalize(V + L);
    
    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V));
    float VoH = saturate(dot(V, H));
    float NoH = saturate(dot(N, H));
    
    float a2 = pow(roughness, 4.0f);
    float D = GGX(a2, NoH);
    float3 F = FresnelSchlick(F0, VoH);
    float G = GeometrySchlickGGX(roughness, NoV) * GeometrySchlickGGX(roughness, NoL);
    
    return (D * G * F) / (4 * max(NoL * NoV, 0.001f));
    
}

float3 LambertDiffuse(float3 diffuseColor)
{
    return diffuseColor * (1 / X_PI);
}

float3 DefaultBRDF(float3 lightDir, float3 normal, float3 viewDir, float roughness, float metallic, float3 baseColor)
{
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor.rgb, metallic);
    
    // Base color remapping
    float3 diffuseColor = (1.0f - metallic) * baseColor;
    
    float3 diffuseBRDF = LambertDiffuse(diffuseColor);
    float3 specularBRDF = SpecularGGX(normal, lightDir, viewDir, roughness, F0);
    
    return diffuseBRDF + specularBRDF;

}

float3 EnvBRDF(float metallic, float3 baseColor, float2 lut)
{
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor.rgb, metallic);
    
    return F0 * lut.x + lut.y;
}

float3 AmbientLighting(float metallic, float3 baseColor,
                       float3 irradiance, float3 prefilterColor,
                       float2 lut, float AmbientAccess)
{
    // IBL diffuse
    float3 diffuseColor = (1.0f - metallic) * baseColor;
    float3 diffuseContribution = diffuseColor * irradiance;
    
    // IBL specular
    float3 specularContribution = prefilterColor * EnvBRDF(metallic, baseColor, lut);
    
    float3 ambient = (diffuseContribution + specularContribution) * AmbientAccess;
    
    return ambient;
}

float3 DirectLighting(float3 radiance, float3 lightDir, float3 normal, float3 viewDir, float roughness, float metallic, float3 baseColor)
{
    float3 BRDF = DefaultBRDF(lightDir, normal, viewDir, roughness, metallic, baseColor);
    
    float NoL = saturate(dot(normal, lightDir));
    
    return radiance * BRDF * NoL;
}



#endif