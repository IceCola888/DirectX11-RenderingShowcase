#ifndef SHADOW_HLSL
#define SHADOW_HLSL

#include "FullScreenTriangle.hlsl"

#ifndef BLUR_KERNEL_SIZE
#define BLUR_KERNEL_SIZE 3
#endif

static const int    BLUR_KERNEL_BEGIN =              BLUR_KERNEL_SIZE / -2;
static const int    BLUR_KERNEL_END =                BLUR_KERNEL_SIZE / 2 + 1;
static const float  FLOAT_BLUR_KERNEL_SIZE = (float) BLUR_KERNEL_SIZE;

struct VertexPosNormalTex
{
    float3 posL :       POSITION;
    float3 normalL :    NORMAL;
    float2 texCoord :   TEXCOORD;
};

struct VertexPosHTex
{
    float4 posH :       SV_POSITION;
    float2 texCoord :   TEXCOORD;
};

cbuffer CB : register(b0)
{
    matrix  g_WorldViewProj;
    float2  g_EvsmExponents;
    float   g_TexelSize;
    float   g_PadTexelSize;
}

cbuffer CBBlur : register(b1)
{
    float4 g_BlurWeightsArray[4];
   
    static float g_BlurWeights[16] = (float[16]) g_BlurWeightsArray;
}

Texture2D g_AlbedoMap :     register(t0);
Texture2D g_TextureShadow : register(t1);

SamplerState g_SamLinearWrap :      register(s0);
SamplerState g_SamplerPointClamp :  register(s1);

float2 GetEVSMExponents(in float positiveExponent, in float negativeExponent)
{
    // オーバーフロー防止の上限
    const float maxExponent = 42.0f;

    float2 exponents = float2(positiveExponent, negativeExponent);

    return min(exponents, maxExponent);
}


// 入力される depth は [0, 1] の範囲である必要がある
float2 ApplyEvsmExponents(float depth, float2 exponents)
{
    depth = 2.0f * depth - 1.0f;
    float2 expDepth;
    expDepth.x = exp(exponents.x * depth);
    expDepth.y = -exp(-exponents.y * depth);
    return expDepth;
}

VertexPosHTex ShadowVS(VertexPosNormalTex vIn)
{
    VertexPosHTex vOut;

    vOut.posH = mul(float4(vIn.posL, 1.0f), g_WorldViewProj);
    vOut.texCoord = vIn.texCoord;

    return vOut;
}


float ShadowPS(VertexPosHTex pIn, uniform float clipValue) : SV_Target
{
    // シャドウマップのカスケードクリッピング
    clip(pIn.posH.z - clipValue);
    return pIn.posH.z;
}

float4 DebugPS(VertexPosHTex pIn) : SV_Target
{
    float depth = g_TextureShadow.Sample(g_SamLinearWrap, pIn.texCoord);
    return float4(depth.rrr, 1.0f);
}

float2 VarianceShadowPS(float4 posH : SV_Position,
                        float2 texCoord : TEXCOORD) : SV_Target
{
    uint2 coords = uint2(posH.xy);
    
    float2 depth;
    depth.x = g_TextureShadow[coords];
    depth.y = depth.x * depth.x;
    
    return depth;
}

float ExponentialShadowPS(float4 posH : SV_Position,
                          float2 texCoord : TEXCOORD,
                           uniform float c) : SV_Target
{
    // float精度の原因 (c*d)を保存
    uint2 coords = uint2(posH.xy);
    return c * g_TextureShadow[coords];
}

float2 EVSM2CompPS(float4 posH : SV_Position,
                   float2 texCoord : TEXCOORD) : SV_Target
{
    uint2 coords = uint2(posH.xy);
    float2 exponents = GetEVSMExponents(g_EvsmExponents.x, g_EvsmExponents.y);
    float2 depth = ApplyEvsmExponents(g_TextureShadow[coords].x, exponents);
    float2 outDepth = float2(depth.x, depth.x * depth.x);
    return outDepth;
}

float4 EVSM4CompPS(float4 posH : SV_Position,
                   float2 texCoord : TEXCOORD) : SV_Target
{
    uint2 coords = uint2(posH.xy);
    float2 depth = ApplyEvsmExponents(g_TextureShadow[coords].x, g_EvsmExponents);
    float4 outDepth = float4(depth, depth * depth).xzyw;
    return outDepth;
}

// ガウスブラー（X方向） - 平均値 μ = E(d)
float4 GaussianBlurXPS(float4 posH : SV_Position,
                         float2 texcoord : TEXCOORD) : SV_Target
{
    float4 depths = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (int x = BLUR_KERNEL_BEGIN; x < BLUR_KERNEL_END; ++x)
    {
        float2 offset = float2(x * g_TexelSize, 0.0f);
        depths += g_BlurWeights[x - BLUR_KERNEL_BEGIN] * g_TextureShadow.SampleLevel(g_SamplerPointClamp, texcoord + offset, 0);
    }
    return depths;
}

// ガウスブラー（Y方向） - 分散用の平方値 E(d^2)
float4 GaussianBlurYPS(float4 posH : SV_Position,
                       float2 texcoord : TEXCOORD) : SV_Target
{
    float4 depths = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (int y = BLUR_KERNEL_BEGIN; y < BLUR_KERNEL_END; ++y)
    {
        float2 offset = float2(0.0f, y * g_TexelSize);
        depths += g_BlurWeights[y - BLUR_KERNEL_BEGIN] * g_TextureShadow.SampleLevel(g_SamplerPointClamp, texcoord + offset, 0);
    }
    return depths;
}
 
// ログガウスブラー（指数空間でブラーをかける）
float LogGaussianBlurPS(float4 posH : SV_Position,
                        float2 texcoord : TEXCOORD) : SV_Target
{
    // 中心の既定値
    float cd0 = g_TextureShadow.Sample(g_SamplerPointClamp, texcoord);
    float sum = g_BlurWeights[FLOAT_BLUR_KERNEL_SIZE / 2] * g_BlurWeights[FLOAT_BLUR_KERNEL_SIZE / 2];
    
    [unroll]
    for (int i = BLUR_KERNEL_BEGIN; i < BLUR_KERNEL_END; ++i)
    {
        for (int j = BLUR_KERNEL_BEGIN; j < BLUR_KERNEL_END; ++j)
        {
            float cdk = g_TextureShadow.Sample(g_SamplerPointClamp, texcoord, int2(i, j)).x * (float) (i != 0 || j != 0);
            sum += g_BlurWeights[i - BLUR_KERNEL_BEGIN] * g_BlurWeights[j - BLUR_KERNEL_BEGIN] * exp(cdk - cd0);
        }

    }
    
    // 最後に指数空間を戻す
    sum = log(sum) + cd0;
    // 無限大を防ぐための上限
    sum = isinf(sum) ? 85.0f : sum; 
    
    return sum;
}
#endif