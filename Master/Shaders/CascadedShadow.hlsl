#ifndef CASCADED_SHADOW_HLSL
#define CASCADED_SHADOW_HLSL

#include "PBRConstants.hlsl"

// 0: Cascaded Shadow Map
// 1: Variance Shadow Map
// 2: Exponential Shadow Map
// 3: Exponential Variance Shadow Map 2-Component
// 4: Exponential Variance Shadow Map 4-Component
#ifndef SHADOW_TYPE
#define SHADOW_TYPE 0
#endif


// 現在のピクセルフラグメントに対して適切なカスケードレベルを選択する方法は2つある：
// Interval-based Selection（区間ベースの選択）では、視錐台の深度分割とピクセルの深度を比較する。
// Map-based Selection（マップベースの選択）では、シャドウマップ上のテクスチャ座標が含まれる最小のカスケードレベルを見つける。
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
#define SELECT_CASCADE_BY_INTERVAL_FLAG 0
#endif


#ifndef CASCADE_COUNT_FLAG
#define CASCADE_COUNT_FLAG 4
#endif

static const float4 s_CascadeColorsMultiplier[8] =
{
    float4(1.5f, 0.0f, 0.0f, 1.0f),
    float4(0.0f, 1.5f, 0.0f, 1.0f),
    float4(0.0f, 0.0f, 5.5f, 1.0f),
    float4(1.5f, 0.0f, 5.5f, 1.0f),
    float4(1.5f, 1.5f, 0.0f, 1.0f),
    float4(1.0f, 1.0f, 1.0f, 1.0f),
    float4(0.0f, 1.0f, 5.5f, 1.0f),
    float4(0.5f, 3.5f, 0.75f, 1.0f)
};

float Linstep(float a, float b, float v)
{
    return saturate((v - a) / (b - a));
}

// [0, amount]の範囲をゼロにし、(amount, 1]の範囲を(0, 1]に再マッピングする
float ReduceLightBleeding(float pMax, float amount)
{
    return Linstep(amount, 1.0f, pMax);
    
}

float2 GetEVSMExponents(float positiveExponent, float negativeExponent)
{
    const float maxExponent = 42.0f;

    float2 exponents = float2(positiveExponent, negativeExponent);

    // オーバーフローを防ぐために指数の範囲を制限する
    return min(exponents, maxExponent);
}

// 入力されるdepthは[0, 1]の範囲である必要がある
float2 ApplyEvsmExponents(float depth, float2 exponents)
{
    depth = 2.0f * depth - 1.0f;
    float2 expDepth;
    expDepth.x = exp(exponents.x * depth);
    expDepth.y = -exp(-exponents.y * depth);
    return expDepth;
}

float ChebyshevUpperBound(float2 moments,
                          float receiverDepth,
                          float minVariance,
                          float lightBleedingReduction)
{
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, minVariance); // 0除算を防ぐため
    
    float d = receiverDepth - moments.x;
    float p_max = variance / (variance + d * d);
    
    p_max = ReduceLightBleeding(p_max, lightBleedingReduction);
    
    // 片側チュビシェフの不等式
    return (receiverDepth <= moments.x ? 1.0f : p_max);
}

//--------------------------------------------------------------------------------------
// カスケードの表示カラーまたは遷移カラーを計算する
//--------------------------------------------------------------------------------------
float4 GetCascadeColorMultipler(int currentCascadeIndex,
                                int nextCascadeIndex,
                                float blendBetweenCascadesAmount)
{
    return lerp(s_CascadeColorsMultiplier[nextCascadeIndex],
                s_CascadeColorsMultiplier[currentCascadeIndex],
                blendBetweenCascadesAmount);
}

//--------------------------------------------------------------------------------------
// PCFサンプリングでシャドウを計算し、影のかかり具合（シェーディングの割合）を返す
//--------------------------------------------------------------------------------------
float CalculatePCFPercentLit(int currentCascadeIndex,
                             float4 shadowTexCoord, float blurSize)
{
    float percentLit = 0.0f;
    
    for (int x = g_PCFBlurForLoopStart; x < g_PCFBlurForLoopEnd; ++x)
    {
        for (int y = g_PCFBlurForLoopStart; y < g_PCFBlurForLoopEnd; ++y)
        {
            float depthCmp = shadowTexCoord.z;
            depthCmp -= g_PCFDepthBias;
            percentLit += g_ShadowMap.SampleCmpLevelZero(g_SamShadowCmp,
                float3(shadowTexCoord.x + (float) x * g_TexelSize,
                        shadowTexCoord.y + (float) y * g_TexelSize,
                        (float) currentCascadeIndex), depthCmp);
        }

    }
    percentLit /= blurSize;
    return percentLit;
}

//--------------------------------------------------------------------------------------
// VSM：シャドウマップをサンプリングし、シェーディング率を返す
//--------------------------------------------------------------------------------------
float CalculateVarianceShadow(float4 shadowTexCoord,
                              float4 shadowTexCoordViewSpace,
                              int currentCascadeIndex,
                              float3 shadowViewDDX,
                              float3 shadowViewDDY)
{
    float percentLit = 0.0f;
    
    float2 moments = 0.0f;
    
    //float3 shadowTexCoordDDX = ddx(shadowTexCoordViewSpace).xyz;
    //float3 shadowTexCoordDDY = ddy(shadowTexCoordViewSpace).xyz;
    //shadowTexCoordDDX *= g_CascadeScale[currentCascadeIndex].xyz;
    //shadowTexCoordDDY *= g_CascadeScale[currentCascadeIndex].xyz;
    
    //
    // CSでddx ddy関数は使えないので、修正する必要があります
    //
    float2 shadowTexCoordDDX = shadowViewDDX.xyz;
    float2 shadowTexCoordDDY = shadowViewDDY.xyz;
    shadowTexCoordDDX *= g_CascadeScale[currentCascadeIndex].xyz;
    shadowTexCoordDDY *= g_CascadeScale[currentCascadeIndex].xyz;
    
    moments += g_ShadowMap.SampleGrad(g_SamShadow,
                   float3(shadowTexCoord.xy, (float) currentCascadeIndex),
                   shadowTexCoordDDX.xy, shadowTexCoordDDY.xy).xy;

    percentLit = ChebyshevUpperBound(moments, shadowTexCoord.z, 0.0004f, g_LightBleedingReduction);
    
    return percentLit;
}

//--------------------------------------------------------------------------------------
// ESM：シャドウマップをサンプリングし、シェーディング率を返す
//--------------------------------------------------------------------------------------
float CalculateExponentialShadow(float4 shadowTexCoord,
                                 float4 shadowTexCoordViewSpace,
                                 int currentCascadeIndex,
                                 float3 shadowViewDDX,
                                 float3 shadowViewDDY)
{
    float percentLit = 0.0f;
    
    float occluder = 0.0f;
    
    //
    // CSでddx ddy関数は使えないので、修正する必要があります
    //
    float3 shadowTexCoordDDX = shadowViewDDX;
    float3 shadowTexCoordDDY = shadowViewDDY;
    shadowTexCoordDDX *= g_CascadeScale[currentCascadeIndex].xyz;
    shadowTexCoordDDY *= g_CascadeScale[currentCascadeIndex].xyz;
    
    occluder += g_ShadowMap.SampleGrad(g_SamShadow,
                    float3(shadowTexCoord.xy, (float) currentCascadeIndex),
                    shadowTexCoordDDX.xy, shadowTexCoordDDY.xy).x;
    
    // exp(cd * -cz)
    percentLit = saturate(exp(occluder - g_MagicPower * shadowTexCoord.z));
    
    return percentLit;
}

//--------------------------------------------------------------------------------------
// EVSM：シャドウマップをサンプリングし、シェーディング率を返す
//--------------------------------------------------------------------------------------
float CalculateExponentialVarianceShadow(float4 shadowTexCoord,
                                         float4 shadowTexCoordViewSpace,
                                         int currentCascadeIndex,
                                         float3 shadowViewDDX,
                                         float3 shadowViewDDY)
{
    float percentLit = 0.0f;
    
    float2 exponents = GetEVSMExponents(g_EvsmPosExp, g_EvsmNegExp);
    float2 expDepth = ApplyEvsmExponents(shadowTexCoord.z, exponents);
    float4 moments = 0.0f;
    
    //
    // CSでddx ddy関数は使えないので、修正する必要があります
    //
    float3 shadowTexCoordDDX = shadowViewDDX;
    float3 shadowTexCoordDDY = shadowViewDDY;
    shadowTexCoordDDX *= g_CascadeScale[currentCascadeIndex].xyz;
    shadowTexCoordDDY *= g_CascadeScale[currentCascadeIndex].xyz;
    
    moments += g_ShadowMap.SampleGrad(g_SamShadow,
                    float3(shadowTexCoord.xy, (float) currentCascadeIndex),
                    shadowTexCoordDDX.xy, shadowTexCoordDDY.xy);
    
    percentLit = ChebyshevUpperBound(moments.xy, expDepth.x, 0.00001f, g_LightBleedingReduction);
    if (SHADOW_TYPE == 4)
    {
        float neg = ChebyshevUpperBound(moments.zw, expDepth.y, 0.00001f, g_LightBleedingReduction);
        percentLit = min(percentLit, neg);
    }
    
    return percentLit;
}



//--------------------------------------------------------------------------------------
// 2つのカスケード間のブレンド量と、ブレンドが発生する領域を計算する
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForInterval(int currentCascadeIndex,
                                     inout float pixelDepth,
                                     inout float currentPixelsBlendBandLocation,
                                     out float blendBetweenCascadesAmount)
{
    //                  pixelDepth
    //           |<-      ->|
    // /-+-------/----------+------/--------
    // 0 N     F[0]               F[i]
    //           |<-blendInterval->|
    // blendBandLocation = 1 - depth/F[0] or
    // blendBandLocation = 1 - (depth-F[0]) / (F[i]-F[0])
    // blendBandLocation が [0, g_CascadeBlendArea] の範囲にある場合、[0, 1] の遷移（ブレンド）を行う
    float blendInterval = g_CascadeFrustumsEyeSpaceDepthsFloat4[currentCascadeIndex].x;
    if (currentCascadeIndex > 0)
    {
        int blendIntervalbelowIndex = currentCascadeIndex - 1;
        pixelDepth -= g_CascadeFrustumsEyeSpaceDepthsFloat4[blendIntervalbelowIndex].x;
        blendInterval -= g_CascadeFrustumsEyeSpaceDepthsFloat4[blendIntervalbelowIndex].x;
    }
    
    // ピクセルのブレンド位置
    currentPixelsBlendBandLocation = 1.0f - pixelDepth / blendInterval;
    // blendBetweenCascadesAmount は最終的な影の色の補間に使われる
    blendBetweenCascadesAmount = currentPixelsBlendBandLocation / g_CascadeBlendArea;
}

void CalculateBlendAmountForMap(float4 shadowMapTexcoord,
                                inout float currentPixelsBlendBandLocation,
                                inout float blendBetweenCascadesAmount)
{
    //   _____________________
    //  |       map[i+1]      |
    //  |                     |
    //  |      0_______0      |
    //  |______| map[i]|______|
    //         |  0.5  |
    //         |_______|
    //         0       0
    // blendBandLocation = min(tx, ty, 1-tx, 1-ty);
    // blendBandLocation が [0, g_CascadeBlendArea] の範囲にある場合、[0, 1] の範囲で遷移（ブレンド）を行う
    float2 distanceToOne = float2(1.0f - shadowMapTexcoord.x, 1.0f - shadowMapTexcoord.y);
    currentPixelsBlendBandLocation = min(shadowMapTexcoord.x, shadowMapTexcoord.y);
    float currentPixelsBlendBandLocation2 = min(distanceToOne.x, distanceToOne.y);
    currentPixelsBlendBandLocation =
        min(currentPixelsBlendBandLocation, currentPixelsBlendBandLocation2);
    
    blendBetweenCascadesAmount = currentPixelsBlendBandLocation / g_CascadeBlendArea;
}

//--------------------------------------------------------------------------------------
// CSM計算
//--------------------------------------------------------------------------------------
float CalculateCascadedShadow(float4 shadowMapTexCoordViewSpace,
                              float currentPixelDepth,
                              float3 shadowViewDDX,
                              float3 shadowViewDDY,
                              out int currentCascadeIndex,
                              out int nextCascadeIndex,
                              out float blendBetweenCascadesAmount)
{
    float4 shadowMapTexCoord = 0.0f;
    float4 shadowMapTexCoord_blend = 0.0f;
    blendBetweenCascadesAmount = 1.0f;
    // 可視化用のカラー
    float4 visualizeCascadeColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    float percentLit = 0.0f;
    float4 percentLit_blend = 0.0f;
    
    float upTextDepthWeight = 0;
    float rightTextDepthWeight = 0;
    float upTextDepthWeight_blend = 0;
    float rightTextDepthWeight_blend = 0;
    
    float blurSize = g_PCFBlurForLoopEnd - g_PCFBlurForLoopStart;
    blurSize *= blurSize;
    
    // 対応するカスケードが見つかったかどうか
    int cascadeFound = 0;
    nextCascadeIndex = 1;
    
    //
    // カスケードを判別し、シャドウマップのテクスチャ座標へ変換
    //
    
    // 視錐台が均等に分割されており、かつ Interval-Based Selection 技術を使用している場合、
    // 対応するカスケードを探すためのループは不要になる
    // このような場合は currentPixelDepth を使って、正しいカスケードインデックスを直接取得できる
    
    // Interval-Based Selection
    if (SELECT_CASCADE_BY_INTERVAL_FLAG)
    {
        currentCascadeIndex = 0;
        //                               Depth
        // /-+-------/----------------/----+-------/----------/
        // 0 N     F[0]     ...      F[i]        F[i+1] ...   F
        // Depth > F[i] to F[0] => index = i+1
        if (CASCADE_COUNT_FLAG > 1)
        {
            // PlayerSpace深度値を取得する
            float4 currentPixelDepthVec = currentPixelDepth;
            
            // 現在のピクセルの深度と、各カスケードに保存されている最大深度値との大小関係を判定する
            float4 cmpVec1 = (currentPixelDepthVec > g_CascadeFrustumsEyeSpaceDepthsFloat[0]);
            float4 cmpVec2 = (currentPixelDepthVec > g_CascadeFrustumsEyeSpaceDepthsFloat[1]);
            
            // 現在のピクセル深度が各カスケードの最大深度とどう比較されるかを判定する
            float index = dot(float4(CASCADE_COUNT_FLAG > 0,
                                     CASCADE_COUNT_FLAG > 1,
                                     CASCADE_COUNT_FLAG > 2,
                                     CASCADE_COUNT_FLAG > 3),
                              cmpVec1) +
                          dot(float4(CASCADE_COUNT_FLAG > 4,
                                     CASCADE_COUNT_FLAG > 5,
                                     CASCADE_COUNT_FLAG > 6,
                                     CASCADE_COUNT_FLAG > 7),
                              cmpVec2);
            index = min(index, CASCADE_COUNT_FLAG - 1);
            currentCascadeIndex = (int) index;
        }
        
        shadowMapTexCoord = shadowMapTexCoordViewSpace * g_CascadeScale[currentCascadeIndex] + g_CascadeOffset[currentCascadeIndex];
    }
    
    // Map-Based Selection
    if (!SELECT_CASCADE_BY_INTERVAL_FLAG)
    {
        currentCascadeIndex = 0;
        if (CASCADE_COUNT_FLAG == 1)
        {
            // カスケードが1つしかない場合、シャドウマップのNDC座標をテクスチャ座標系に変換する
            shadowMapTexCoord = shadowMapTexCoordViewSpace * g_CascadeScale[0] + g_CascadeOffset[0];
        }
        if (CASCADE_COUNT_FLAG > 1)
        {
            // テクスチャ座標がテクスチャの境界内に収まる最も近いカスケードを探す
            // minBorder < tx, ty < maxBorder
            for (int cascadeIndex = 0; cascadeIndex < CASCADE_COUNT_FLAG && cascadeFound == 0; ++cascadeIndex)
            {
                shadowMapTexCoord = shadowMapTexCoordViewSpace * g_CascadeScale[cascadeIndex] + g_CascadeOffset[cascadeIndex];
                if (min(shadowMapTexCoord.x, shadowMapTexCoord.y) > g_MinBorderPadding
                    && max(shadowMapTexCoord.x, shadowMapTexCoord.y) < g_MaxBorderPadding)
                {
                    currentCascadeIndex = cascadeIndex;
                    cascadeFound = 1;
                }

            }

        }
    }
    
    //
    // 現在のカスケードのPCFを計算する
    //
    visualizeCascadeColor = s_CascadeColorsMultiplier[currentCascadeIndex];
    
    if (SHADOW_TYPE == 0)
        percentLit = CalculatePCFPercentLit(currentCascadeIndex, shadowMapTexCoord, blurSize);
    if (SHADOW_TYPE == 1)
        percentLit = CalculateVarianceShadow(shadowMapTexCoord, shadowMapTexCoordViewSpace, currentCascadeIndex, shadowViewDDX, shadowViewDDY);
    if (SHADOW_TYPE == 2)
        percentLit = CalculateExponentialShadow(shadowMapTexCoord, shadowMapTexCoordViewSpace, currentCascadeIndex, shadowViewDDX, shadowViewDDY);
    if (SHADOW_TYPE >= 3)
        percentLit = CalculateExponentialVarianceShadow(shadowMapTexCoord, shadowMapTexCoordViewSpace, currentCascadeIndex, shadowViewDDX, shadowViewDDY);
    
    //
    // 2つのカスケード間でブレンドを行う
    //
    
    // 次のカスケードに対しても、投影テクスチャ座標の計算を繰り返す  
    // 次のカスケードのインデックスは、2つのカスケード間のブラー処理に使用される

    nextCascadeIndex = min(CASCADE_COUNT_FLAG - 1, currentCascadeIndex + 1);

    
    blendBetweenCascadesAmount = 1.0f;
    float currentPixelsBlendBandLocation = 1.0f;
    if (SELECT_CASCADE_BY_INTERVAL_FLAG)
    {
        if (CASCADE_COUNT_FLAG > 1)
        {
            CalculateBlendAmountForInterval(currentCascadeIndex, currentPixelDepth,
                currentPixelsBlendBandLocation, blendBetweenCascadesAmount);
        }

    }
    else
    {
        if (CASCADE_COUNT_FLAG > 1)
        {
            CalculateBlendAmountForMap(shadowMapTexCoord,
                currentPixelsBlendBandLocation, blendBetweenCascadesAmount);
        }
    }
    
    if (CASCADE_COUNT_FLAG > 1)
    {
        if (currentPixelsBlendBandLocation < g_CascadeBlendArea)
        {
            // 次のカスケードの投影テクスチャ座標を計算する
            shadowMapTexCoord_blend = shadowMapTexCoordViewSpace * g_CascadeScale[nextCascadeIndex] + g_CascadeOffset[nextCascadeIndex];
            
            // カスケード間でブレンドを行う際に、次のカスケードに対しても計算を行う
            if (currentPixelsBlendBandLocation < g_CascadeBlendArea)
            {
                if (SHADOW_TYPE == 0)
                    percentLit_blend = CalculatePCFPercentLit(nextCascadeIndex, shadowMapTexCoord_blend, blurSize);
                if (SHADOW_TYPE == 1)             
                    percentLit_blend = CalculateVarianceShadow(shadowMapTexCoord_blend, shadowMapTexCoordViewSpace, nextCascadeIndex, shadowViewDDX, shadowViewDDY);
                if (SHADOW_TYPE == 2)
                    percentLit_blend = CalculateExponentialShadow(shadowMapTexCoord_blend, shadowMapTexCoordViewSpace, nextCascadeIndex, shadowViewDDX, shadowViewDDY);
                if (SHADOW_TYPE >= 3)
                    percentLit_blend = CalculateExponentialVarianceShadow(shadowMapTexCoord, shadowMapTexCoordViewSpace, currentCascadeIndex, shadowViewDDX, shadowViewDDY);
                // 2つのカスケードのPCF結果をブレンドする
                percentLit = lerp(percentLit_blend, percentLit, blendBetweenCascadesAmount);
            }

        }

    }
    
    return percentLit;
}

#endif