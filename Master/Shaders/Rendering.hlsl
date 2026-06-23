#ifndef RENDERING_HLSL
#define RENDERING_HLSL

#include "PBRConstants.hlsl"
#include "CascadedShadow.hlsl"

#ifndef MSAA_SAMPLES
#define MSAA_SAMPLES 1
#endif

struct GBuffer
{
    float4 normalRoughnessMetallic : SV_Target0;
    float4 albedo : SV_Target1;
    float2 posZGrad : SV_Target2;
};

// 上記のGBufferに加え、深度バッファを含む（最後の要素）  t10-t13
Texture2DMS<float4, MSAA_SAMPLES> g_GBufferTextures[4] : register(t10);




// 法線のエンコード
float2 EncodeSphereMap(float3 normal)
{
    return normalize(normal.xy) * (sqrt(-normal.z * 0.5f + 0.5f));
}

// 法線のデコード
float3 DecodeSphereMap(float2 encoded)
{
    float4 nn = float4(encoded, 1, -1);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return nn.xyz * 2 + float3(0, 0, -1);
}



struct SurfaceData
{
    float3 posV;
    float  roughness;
    
    float3 normalV;
    float  metallic;
    
    float4 albedo;
    
    float3 posV_DX;
    float  ambientOcclusion;
    
    float3 posV_DY;
    float  blendWeight;
    
    int cascadeIndex;
    int nextCascadeIndex;
};

struct VertexPosHVNormalVTex
{
    float4 posH :     SV_Position;
    float3 posV :     Position;
    float3 normalV :  NORMAL;
    float4 tangentV : TANGENT;
    float2 tex :      TEXCOORD;
};

// NDC座標と視点空間のZ値から、視点空間の位置を復元する
float3 ComputePositionViewFromZ(float2 posNdc, float viewSpaceZ)
{
    float2 screenSpaceRay = float2(posNdc.x / g_Proj._m00,
                                   posNdc.y / g_Proj._m11);
    
    float3 posV;
    posV.z = viewSpaceZ;
    posV.xy = screenSpaceRay.xy * posV.z;
    
    return posV;
}

float3 GetPrefilteredColor(float roughness, float3 reflectDir)
{
    float level = roughness * 5;
    
    return g_IBLPrefilterMap.SampleLevel(g_SamLinearClamp, reflectDir, level).rgb;
}

float3 TransformNormalSample(float3 normalMapSample,
    float3 unitNormal, float4 tangent)
{
    // 法線マップから読み込んだ値を [0, 1] → [-1, 1] に変換
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // シュミット直交化で法線と直交する接線を計算
    float3 N = unitNormal;
    float3 T = normalize(tangent.xyz - dot(tangent.xyz, N) * N); 
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    
    // 切線空間の法線をワールド空間へ変換
    float3 bumpedNormal = mul(normalT, TBN);
    
    return normalize(bumpedNormal);
}

float3 TransformNormalSample(float3 normalMapSample, float3 unitNormal, float3 pos, float2 texcoord)
{
    // 法線マップから読み込んだ値を [0, 1] → [-1, 1] に変換
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // 偏微分を用いてポリゴンのタンジェントとバイタンジェントを計算する
    float3 Q1 = ddx_coarse(pos);
    float3 Q2 = ddy_coarse(pos);
    float2 st1 = ddx_coarse(texcoord);
    float2 st2 = ddy_coarse(texcoord);

    float3 N = unitNormal;
    float3 T = normalize(Q1 * st2.y - Q2 * st2.x);
    float3 B = normalize(cross(N, T));

    float3x3 TBN = float3x3(T, B, N);

    // 切線空間の法線をワールド空間へ変換
    float3 bumpedNormal = mul(normalT, TBN);

    return normalize(bumpedNormal);
}

VertexPosHVNormalVTex GeometryVS(VertexPosNormalTangentTex input)
{
    VertexPosHVNormalVTex output;
    
    output.posH = mul(float4(input.posL, 1.0f), g_WorldViewProj);
    output.posV = mul(float4(input.posL, 1.0f), g_WorldView).xyz;
    output.normalV = normalize(mul(input.normalL, (float3x3) g_WorldInvTransposeView));
    output.tangentV = float4(mul(input.tangentL.xyz, (float3x3) g_WorldView), input.tangentL.w);

    output.tex = input.tex;
    
    return output;
}

SurfaceData ComputeSurfaceDataFromGeometry(VertexPosHVNormalVTex v)
{
    SurfaceData surface = (SurfaceData) 0.0f;
    
    surface.posV = v.posV;
    surface.albedo = g_kAlbedo * g_AlbedoMap.Sample(g_SamLinearWrap, v.tex);
    
    float3 normalSample = g_NormalMap.Sample(g_SamLinearWrap, v.tex).xyz;
    surface.normalV = normalize(TransformNormalSample(normalSample, v.normalV, v.posV, v.tex).xyz);

#ifdef USE_MIXED_ARM_MAP
    float3 rm = g_RoughnessMetallicMap.Sample(g_SamLinearWrap, v.tex).rgb;
    surface.roughness = g_kRoughness * rm.g;
    surface.metallic = g_kMetallic * rm.b;
#else
    surface.roughness = g_kRoughness * g_RoughnessMap.Sample(g_SamLinearWrap, v.tex).r;
    surface.metallic = g_kMetallic * g_MetallicMap.Sample(g_SamLinearWrap, v.tex).r;
#endif 

    surface.posV_DX = ddx_fine(surface.posV);
    surface.posV_DY = ddy_fine(surface.posV);
    
    surface.blendWeight = 0.0f;
    surface.cascadeIndex = 0;
    surface.nextCascadeIndex = 0;
    
    return surface;
}

SurfaceData ComputeSurfaceDataFromGBufferSample(uint2 posViewport, uint sampleIndex)
{
    // GBufferからデータを読み込む
    GBuffer rawData;
    
    rawData.normalRoughnessMetallic = g_GBufferTextures[0].Load(posViewport.xy, sampleIndex).xyzw;
    rawData.albedo = g_GBufferTextures[1].Load(posViewport.xy, sampleIndex).xyzw;
    rawData.posZGrad = g_GBufferTextures[2].Load(posViewport.xy, sampleIndex).xy;
    float zBuffer = g_GBufferTextures[3].Load(posViewport.xy, sampleIndex).x;
    
    float2 gbufferDim;
    float dummy;
    // Texture2DMSの場合はスクリーンのピクセル数を取得する（dummyは使用しない）
    g_GBufferTextures[0].GetDimensions(gbufferDim.x, gbufferDim.y, dummy);
    
    // スクリーン／クリップ空間の座標と隣接位置を計算する
    // 注意：DX11のビューポート変換では、ピクセルの中心は(x+0.5, y+0.5)の位置にある
    // 注意：このオフセットは実際にはCPU側で事前に計算可能だが、
    // 定数バッファから読み込むよりもここで再計算した方が高速になることがある
    float2 screenPixelOffset = float2(2.0f, -2.0f) / gbufferDim;
    // ピクセル中心の補正を考慮して、スクリーン座標をNDC空間へ変換する
    float2 posNdc = (float2(posViewport.xy) + 0.5f) * screenPixelOffset.xy + float2(-1.0f, 1.0f);
    // 現在のピクセルの右側にあるピクセルのNDC座標を計算する
    float2 posNdcX = posNdc + float2(screenPixelOffset.x, 0.0f);
    // 現在のピクセルの下側にあるピクセルのNDC座標を計算する
    float2 posNdcY = posNdc + float2(0.0f, screenPixelOffset.y);
    
    // 出力に合わせてデコードを行う
    SurfaceData surfaceData;
        
    // 深度バッファのZ値をビュー空間に逆投影する
    float viewSpaceZ = g_Proj._m32 / (zBuffer - g_Proj._m22);
    
    // NDC空間の座標とカメラ空間の深度からカメラ空間の位置座標を再構成する
    surfaceData.posV = ComputePositionViewFromZ(posNdc, viewSpaceZ);
    // 再構成された2x2ピクセルのうち、右側／下側のビュー空間座標
    surfaceData.posV_DX = ComputePositionViewFromZ(posNdcX, viewSpaceZ + rawData.posZGrad.x) - surfaceData.posV;
    surfaceData.posV_DY = ComputePositionViewFromZ(posNdcY, viewSpaceZ + rawData.posZGrad.y) - surfaceData.posV;
    // ビュー空間法線のデコード
    surfaceData.normalV = DecodeSphereMap(rawData.normalRoughnessMetallic.xy);
    surfaceData.roughness = rawData.normalRoughnessMetallic.z;
    surfaceData.metallic = rawData.normalRoughnessMetallic.w;
    surfaceData.albedo = rawData.albedo;
    // SSAO結果の代入
    float4 posH = mul(float4(surfaceData.posV, 1.0f), g_Proj);
    posH = (posH + float4(posH.w, -posH.w, 0.0f, 0.0f)) * float4(0.5f, -0.5f, 1.0f, 1.0f);
    posH /= posH.w;
    surfaceData.ambientOcclusion = g_SSAOTexture.SampleLevel(g_SamPointClamp, posH.xy, 0.0f).r;
    
    // Shadow
    // CSMは問題ないと思いますが、DDXとDDYは使えないので修正必要ある
    float3 posW = mul(float4(surfaceData.posV, 1.0f), g_InvView).xyz;
    float4 shadowView = mul(float4(posW, 1.0f), g_ShadowView);

    float3 posW_Left = mul(float4(surfaceData.posV - surfaceData.posV_DX, 1.0f), g_InvView).xyz;
    float3 posW_Right = mul(float4(surfaceData.posV + surfaceData.posV_DX, 1.0f), g_InvView).xyz;
    float3 posW_Top = mul(float4(surfaceData.posV - surfaceData.posV_DY, 1.0f), g_InvView).xyz;
    float3 posW_Bottom = mul(float4(surfaceData.posV + surfaceData.posV_DY, 1.0f), g_InvView).xyz;
    
    float4 shadowView_Left = mul(float4(posW_Left, 1.0f), g_ShadowView);
    float4 shadowView_Right = mul(float4(posW_Right, 1.0f), g_ShadowView);
    float4 shadowView_Top = mul(float4(posW_Top, 1.0f), g_ShadowView);
    float4 shadowView_Bottom = mul(float4(posW_Bottom, 1.0f), g_ShadowView);
    
    shadowView_Left = shadowView_Left / shadowView_Left.w * 0.5f + 0.5f;
    shadowView_Right = shadowView_Right / shadowView_Right.w * 0.5f + 0.5f;
    shadowView_Top = shadowView_Top / shadowView_Top.w * 0.5f + 0.5f;
    shadowView_Bottom = shadowView_Bottom / shadowView_Bottom.w * 0.5f + 0.5f;
    
    float3 shadowViewDDX = (shadowView_Right.xyz - shadowView_Left.xyz) * 0.5f;
    float3 shadowViewDDY = (shadowView_Bottom.xyz - shadowView_Top.xyz) * 0.5f;
    
    surfaceData.albedo.w = CalculateCascadedShadow(shadowView, viewSpaceZ, shadowViewDDX, shadowViewDDY, surfaceData.cascadeIndex, surfaceData.nextCascadeIndex, surfaceData.blendWeight);
    
    return surfaceData;
}




#endif