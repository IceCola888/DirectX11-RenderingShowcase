#ifndef BRDF_LUT_HLSL
#define BRDF_LUT_HLSL

#include "Constants.hlsl"

void FullScreenTriangleTexcoordVS(uint vertexID : SV_VertexID,
                                  out float4 posH : SV_Position,
                                  out float2 texcoord : TEXCOORD)
{
    float2 grid = float2((vertexID << 1) & 2, vertexID & 2);
    float2 xy = grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    texcoord = grid * float2(1.0f, 1.0f);
    posH = float4(xy, 1.0f, 1.0f);
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// Schlick-GGXによる近似の幾何項
float GeometrySchlickGGX(float NdotV, float roughness)
{
    // note that we use a different k for IBL
    float a = roughness;
    float k = (a * a) / 2.0f;

    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / denom;

}

// Smithの幾何項
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// GGXに基づく重要度サンプリング
float3 ImportanceSampleGGX(float2 Xi, float Roughness, float3 N)
{
    float a = Roughness * Roughness;
    float phi = X_2PI * Xi.x;
    float cosTheta = saturate(sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y)));
    float sinTheta = saturate(sqrt(1.0 - cosTheta * cosTheta));
	
	// from spherical coordinates to cartesian coordinates - halfway vector
    float3 H = float3(0.0f, 0.0f, 0.0f);
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
	// from tangent-space H vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
	
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// BRDFの積分
float2 IntegrateBRDF(float NdotV, float roughness)
{
    
    float3 V;
    V.x = saturate(sqrt(1.0f - NdotV * NdotV)); // sin
    V.y = 0.0f;
    V.z = NdotV; // cos
    
    float A = 0.0f;
    float B = 0.0f;
    
    const uint NumSamples = 1024u;
    float3 N = float3(0.0f, 0.0f, 1.0f);
    
    for (uint i = 0u; i < NumSamples; ++i)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = normalize(2 * dot(V, H) * H - V);
        
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
       
        
        if (NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;

        }
    }
    A /= float(NumSamples);
    B /= float(NumSamples);
    
    return float2(A, B);
    

}

float4 BRDF_LUT_PS(float4 posH : SV_Position,
                   float2 texcoord : TEXCOORD) : SV_Target
{
    float2 res = IntegrateBRDF(texcoord.x, texcoord.y);
    return float4(res, 0.0f, 1.0f);
    
}

#endif