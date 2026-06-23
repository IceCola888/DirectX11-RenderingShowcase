#ifndef PBRCONSTANTS_HLSL
#define PBRCONSTANTS_HLSL

#include "PBRLighting.hlsl"

Texture2D<float4> g_AlbedoMap :            register(t0);
Texture2D<float>  g_AOMap :                register(t1);
Texture2D<float>  g_DisplacementMap :      register(t2);
Texture2D<float>  g_MetallicMap :          register(t3);
Texture2D<float>  g_RoughnessMap :         register(t4);
Texture2D<float4> g_RoughnessMetallicMap : register(t5);
Texture2D<float4> g_NormalMap :            register(t6);

TextureCube<float4> g_IBLIrradianceMap :   register(t7);
TextureCube<float4> g_IBLPrefilterMap :    register(t8);
Texture2D<float2> g_IBLBRDFLUT :           register(t9);

StructuredBuffer<PointLight> g_PointLight : register(t14);
StructuredBuffer<SpotLight> g_SpotLight :   register(t15);
Texture2DArray g_ShadowMap :                register(t16);
Texture2D<float> g_SSAOTexture :            register(t17);

struct VertexPosNormalTex
{
    float3 posL :     POSITION;
    float3 normalL :  NORMAL;
    float2 texCoord : TEXCOORD;
};

struct VertexPosNormalTangentTex
{
    float3 posL :       POSITION;
    float3 normalL :    NORMAL;
    float4 tangentL :   TANGENT;
    float2 tex :        TEXCOORD;
};

struct VertexPosHWNormalTangentTex
{
    float4 posH :      SV_POSITION;
    float3 posW :      POSITION; 
    float3 normalW :   NORMAL; 
    float4 tangentW :  TANGENT; 
    float2 tex :       TEXCOORD;
};

cbuffer CBChangesEveryDrawing : register(b0)
{
    matrix g_Proj;
    matrix g_World;
    matrix g_InvView;
    matrix g_WorldView;
    matrix g_WorldViewProj;
    matrix g_WorldInvTranspose;
    matrix g_WorldInvTransposeView;
    
    float4 g_kAlbedo;
    float  g_kRoughness;
    float  g_kMetallic;
    uint   g_VisualizeLightCount;
    uint   g_VisualizePerSampleShading;
}

cbuffer CSCascadedShadow : register(b1)
{
    matrix g_ShadowView;
    float4 g_CascadeOffset[8];      // ShadowPT行列の平行移動量
    float4 g_CascadeScale[8];       // ShadowPT行列のスケーリング量
    float4 g_CascadeFrustumsEyeSpaceDepthsFloat[2];   // 各カスケードの遠平面のZ値（視点空間）。これによりカスケードを分割する
    float4 g_CascadeFrustumsEyeSpaceDepthsFloat4[8];  // float4形式で余分なスペースを使って配列ループに対応。yzw成分は未使用
    int    g_PCFBlurForLoopStart;   // PCFブラーのループ開始値。5x5カーネルでは-2
    int    g_PCFBlurForLoopEnd;     // PCFブラーのループ終了値。5x5カーネルでは3
    
    // Map-based Selection手法で使用。影の有効範囲内のピクセルを制限するため
    // 境界がない場合、Minは0、Maxは1になる
    float g_MinBorderPadding;       // (kernelSize / 2) / (float)shadowSize
    float g_MaxBorderPadding;       // 1.0f - (kernelSize / 2) / (float)shadowSize
    
    float g_PCFDepthBias;           // シャドウのアーティファクト対策の深度バイアス。PCF使用時に悪化しやすい
    float g_CascadeBlendArea;       // カスケード間のオーバーラップを補間するためのブレンド領域
    float g_TexelSize;              // シャドウマップにおける1ピクセルのサイズ
    int g_VisualizeCascades;        // 1: カスケードごとに色を変えて可視化、0: 通常のシャドウ描画
    
    float g_LightBleedingReduction; // VSMにおけるライトリークを抑える係数
    float g_EvsmPosExp;             // EVSMで使用する正の指数項
    float g_EvsmNegExp;             // EVSMで使用する負の指数項
    float g_MagicPower;             // ライトリーク抑制のための指数
}

cbuffer CBChangesEveryFrame : register(b2)
{
    matrix g_ViewProj;
    
    float3 g_EyePosW;
    float  g_HeightScale;

    uint   g_UseIBL;
    uint3  g_PadUseIBL;
}

cbuffer CBChangesRarely : register(b3)
{
    float g_MaxTessDistance;
    float g_MinTessDistance;
    float g_MinTessFactor;
    float g_MaxTessFactor;
    
    uint2  g_FramebufferDimensions;
    float2 g_CameraNearFar;
}

cbuffer CBLight : register(b4)
{
    float3 g_DirLightDir;
    float  g_DirLightIntensity;
    float3 g_DirLightColor;
    float  g_PadDirLightColor;
}

SamplerState g_SamLinearClamp :         register(s0);
SamplerState g_SamPointClamp :          register(s1);
SamplerState g_SamLinearWrap :          register(s2);
SamplerState g_SamAnistropicWrap16x :   register(s3);
SamplerState g_SamShadow :              register(s4);
SamplerComparisonState g_SamShadowCmp : register(s5);
#endif