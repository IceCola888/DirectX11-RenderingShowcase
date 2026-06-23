#ifndef COMPUTE_SHADER_TILE_HLSL
#define COMPUTE_SHADER_TILE_HLSL

#include "GBuffer.hlsl"
#include "FramebufferFlat.hlsl"
#include "CascadedShadow.hlsl"
#include "..\\ShaderDefines.h"

// タイルベースのライティング除去のためにタイルサイズを決定し、それに伴うトレードオフを考慮する 
groupshared uint gs_MinZ;
groupshared uint gs_MaxZ;

// 現在のタイルに対応するライトリスト
groupshared uint gs_TileLightIndices[MAX_LIGHTS >> 3];
groupshared uint gs_TileNumLights;

// タイル内でサンプル単位のシェーディングが必要なピクセルのリスト
// メモリ節約のため、x/yの16ビット座標を1つのuintにエンコードする
groupshared uint gs_PerSamplePixels[COMPUTE_SHADER_TILE_GROUP_SIZE];
groupshared uint gs_NumPerSamplePixels;

RWStructuredBuffer<uint2> g_Framebuffer : register(u0);

// 2つの16ビット以下の座標値を1つのuintにパックする
uint PackCoords(uint2 coords)
{
    return coords.y << 16 | coords.x;
}

// 1つのuintを2つの16ビット以下の座標値にアンパックする
uint2 UnpackCoords(uint coords)
{
    return uint2(coords & 0xFFFF, coords >> 16);
}

//--------------------------------------------------------------------------------------
// 1DのMSAA UAVに書き込むために使用する
void WriteSample(uint2 coords, uint sampleIndex, float4 value)
{
    // FrameBuffer のメモリレイアウトは sampleIndex ごとにセグメント化されている
    // 各セグメントは、1つの MSAA サンプルに対応するすべてのピクセルデータを格納している
    g_Framebuffer[GetFramebufferSampleAddress(coords, sampleIndex)] = PackRGBA16(value);
}

void ConstructFrustumPlanes(uint3 groupId, float minTileZ, float maxTileZ,
                            out float4 frustumPlanes[6])
{
    // 注意：以下の計算は各タイルで共通であり（つまり、すべてのスレッドが実行する必要はない）、コストも低い。
    // 各タイルごとに視錐台平面をあらかじめ計算し、結果を定数バッファに格納しておくことも可能。
    // これはビュー空間で行われるため、投影行列が変更されたときのみ再計算が必要になる。
    // そして実際のジオメトリに沿って、ニア／ファークリップ面だけを調整すればよい。
    // いずれにせよ、グループ同期やローカルデータ共有（LDS）、またはグローバルメモリアクセスによるオーバーヘッドは、
    // この程度の数学的処理と大差ない可能性があるため、一度試してみる価値はある。
    
    // 元のIntelのサンプルで計算されていたScaleとBiasには誤りがあるため、ここで改めて導出し直した
    float2 tileScale = float2(g_FramebufferDimensions.xy) / COMPUTE_SHADER_TILE_GROUP_DIM;
    float2 tileBias = tileScale - 1.0f - 2.0f * float2(groupId.xy);
    
    // 現在のタイルの視錐台に対応する投影行列を計算する
    float4 c1 = float4(g_Proj._11 * tileScale.x, 0.0f, tileBias.x, 0.0f);
    float4 c2 = float4(0.0f, g_Proj._22 * tileScale.y, -tileBias.y, 0.0f);
    float4 c4 = float4(0.0f, 0.0f, 1.0f, 0.0f);
    
    // Gribb/Hartmann法を使用して視錐台の平面を抽出する
    // 側面
    frustumPlanes[0] = c4 - c1; // 右クリッピング平面 
    frustumPlanes[1] = c4 + c1; // 左クリッピング平面
    frustumPlanes[2] = c4 - c2; // 上クリッピング平面
    frustumPlanes[3] = c4 + c2; // 下クリッピング平面
    // 近 / 遠クリッピング平面
    frustumPlanes[4] = float4(0.0f, 0.0f, 1.0f, -minTileZ);
    frustumPlanes[5] = float4(0.0f, 0.0f, -1.0f, maxTileZ);
    
    // 標準化された視錐台平面（近/遠平面はすでに正規化済み）
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        frustumPlanes[i] *= rcp(length(frustumPlanes[i].xyz));
    }
}


float3 ComputeIBLAmbientLight(SurfaceData surfaceSample)
{
    float3 ambient = float3(0.0f, 0.0f, 0.0f);
    // シェーディング点からカメラへのベクトル
    float3 viewDir = normalize(-surfaceSample.posV);
    viewDir = mul(viewDir, (float3x3) g_InvView);
    float3 normalW = mul(surfaceSample.normalV, (float3x3) g_InvView);
    // Irradiance
    float3 irradiance = g_IBLIrradianceMap.SampleLevel(g_SamLinearClamp, normalW, 0).xyz;
    // PrefilteredColor
    float3 reflectDir = reflect(-viewDir, normalW);
    float3 prefilteredColor = GetPrefilteredColor(surfaceSample.roughness, reflectDir);
    // BRDFLUT
    float NoV = dot(normalW, viewDir);
    float2 lut = g_IBLBRDFLUT.SampleLevel(g_SamLinearClamp, float2(NoV, surfaceSample.roughness), 0).rg;
    ambient = AmbientLighting(surfaceSample.metallic, surfaceSample.albedo.rgb, irradiance, prefilteredColor, lut, 1.0f);
    
    return ambient;
}

float3 ComputeDirectionalLight(SurfaceData surfaceSample)
{
    float3 lit = float3(0.0f, 0.0f, 0.0f);
    float3 viewDir = normalize(-surfaceSample.posV);
    float3 dirLightRadiance = g_DirLightIntensity * g_DirLightColor.rgb;
    // 平行光源による直接照明の計算
    lit = DirectLighting(dirLightRadiance, -g_DirLightDir, surfaceSample.normalV, viewDir, surfaceSample.roughness, surfaceSample.metallic, surfaceSample.albedo.rgb);
    
    return lit;
}

float3 ComputePointLight(SurfaceData surfaceSample, uint tileLightIndex)
{
    float3 lit = float3(0.0f, 0.0f, 0.0f);
    // タイルに対応する点光源の情報を取得
    PointLight pointLight = g_PointLight[gs_TileLightIndices[tileLightIndex]];
    // シェーディング点からカメラへの視線ベクトル
    float3 viewDir = normalize(-surfaceSample.posV);
    // シェーディング点から点光源へのベクトル
    float3 pointLightDir = normalize(pointLight.position - surfaceSample.posV);
    // 点光源の距離減衰係数を計算
    float attenuation = CalcDistanceAttenuation(surfaceSample.posV, pointLight.position, pointLight.range);
    // 点光源の放射輝度（強度 × 減衰 × 色）
    float3 pointLightRadiance = pointLight.intensity * attenuation * pointLight.color;
    // 直接照明を加算
    lit = DirectLighting(pointLightRadiance, pointLightDir, surfaceSample.normalV, viewDir, surfaceSample.roughness, surfaceSample.metallic, surfaceSample.albedo.rgb);
    return lit;
}

[numthreads(COMPUTE_SHADER_TILE_GROUP_DIM, COMPUTE_SHADER_TILE_GROUP_DIM, 1)]
void ComputeShaderTileDeferredCS(uint3 groupID :          SV_GroupID,
                                 uint3 dispatchThreadID : SV_DispatchThreadID,
                                 uint3 groupThreadID :    SV_GroupThreadID,
                                 uint  groupIndex :       SV_GroupIndex)
{
    //
    // サーフェスデータを取得し、現在のタイルの視錐台を計算して、深度範囲を決定する
    //
    
    // 現在のピクセル位置を表す
    uint2 globalCoords = dispatchThreadID.xy;
    
    // すべてのサーフェス情報をアンパックする
    SurfaceData surfaceSamples[MSAA_SAMPLES];
    ComputeSurfaceDataFromGBufferAllSamples(globalCoords, surfaceSamples);
  
    // すべてのサンプルにおけるZの範囲を探す
    float minZSample = g_CameraNearFar.y;
    float maxZSample = g_CameraNearFar.x;
    {
        [unroll]
        for (uint sample = 0; sample < MSAA_SAMPLES; sample++)
        {
            // スカイボックスやその他の無効なピクセルにシェーディングしないようにする
            float viewSpaceZ = surfaceSamples[sample].posV.z;
            bool validPixel = viewSpaceZ >= g_CameraNearFar.x &&
                              viewSpaceZ < g_CameraNearFar.y;
            
            [flatten]
            if(validPixel)
            {
                // 最も遠い点に対応するのは最小の深度値
                minZSample = min(minZSample, viewSpaceZ);
                // 最も近い点に対応するのは最大の深度値
                maxZSample = max(maxZSample, viewSpaceZ);
            }
        }

    }
    
    // 共有メモリ内のライトリストとZ範囲を初期化する
    if (groupIndex == 0)
    {
        gs_TileNumLights = 0;
        gs_NumPerSamplePixels = 0;
        gs_MinZ = 0x7F7FFFFF;       // 浮動小数点数の最大値
        gs_MaxZ = 0;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // 最大深度値が最小深度値以上であることを保証する
    if (maxZSample >= minZSample)
    {
        InterlockedMin(gs_MinZ, asuint(minZSample));
        InterlockedMax(gs_MaxZ, asuint(maxZSample));
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    float minTileZ = asfloat(gs_MinZ);
    float maxTileZ = asfloat(gs_MaxZ);
    float4 frustumPlanes[6];
    ConstructFrustumPlanes(groupID, minTileZ, maxTileZ, frustumPlanes);
    
    //
    // 現在のタイルに対してライティングのカリングを行う
    //
    
    uint totalLights, dummy;
    g_PointLight.GetDimensions(totalLights, dummy);
    
    // グループ内の各スレッドが一部のライトとの衝突判定を担当する
    for (uint lightIndex = groupIndex; lightIndex < totalLights; lightIndex += COMPUTE_SHADER_TILE_GROUP_SIZE)
    {
        PointLight light = g_PointLight[lightIndex];
                
        // 点光源の球体とタイルの視錐台との当たり判定
        bool inFrustum = true;
        [unroll]
        for (uint i = 0; i < 6; ++i)
        {
            float d = dot(frustumPlanes[i], float4(light.position, 1.0f));
            inFrustum = inFrustum && (d >= -light.range);
        }

        [branch]
        if (inFrustum)
        {
            // ライトをリストに追加する
            uint listIndex;
            InterlockedAdd(gs_TileNumLights, 1, listIndex);
            gs_TileLightIndices[listIndex] = lightIndex;
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    
    //
    // 画面領域内のピクセルのみを処理する（タイルが画面端を越える可能性がある）
    // 
    
    // 現在のタイル内のポイントライトの数を取得する
    uint numLights = gs_TileNumLights;
    // 現在のピクセル座標がスクリーン範囲内であることを確認する
    if (all(globalCoords < g_FramebufferDimensions.xy))
    {
        
        // タイルの可視化が有効かどうかを確認する
        [branch]
        if (g_VisualizeLightCount)
        {
            [unroll]
            for (uint sample = 0; sample < MSAA_SAMPLES; sample++)
            {
                float4 red = float4(1.0f, 0.0f, 0.0f, 1.0f);
                float4 green = float4(0.0f, 1.0f, 0.0f, 1.0f);
                float4 blue = float4(0.0f, 0.0f, 1.0f, 1.0f);
                float light = float(gs_TileNumLights) / 255.0f;
                if (gs_TileNumLights > 0 && gs_TileNumLights <= 16)
                    WriteSample(globalCoords, sample, light);
                else if (gs_TileNumLights > 16 && gs_TileNumLights <= 32)
                    WriteSample(globalCoords, sample, light * blue);
                else if (gs_TileNumLights > 32 && gs_TileNumLights <= 64)
                    WriteSample(globalCoords, sample, light * (blue + green));
                else if (gs_TileNumLights > 64 && gs_TileNumLights <= 128)
                    WriteSample(globalCoords, sample, light * green);
                else if (gs_TileNumLights > 128 && gs_TileNumLights <= 256) 
                    WriteSample(globalCoords, sample, light * (green + red));
                else if (gs_TileNumLights > 256 && gs_TileNumLights <= 512) 
                    WriteSample(globalCoords, sample, light * red);
                else
                    WriteSample(globalCoords, sample, light * float4(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
        // タイルの可視化が無効で、かつそのタイルにライトが存在する場合
        else if (numLights > 0)
        {
            // 境界をマークする
            bool perSampleShading = RequiresPerSampleShading(surfaceSamples);
            // 境界の可視化が有効な場合
            [branch]
            if (g_VisualizePerSampleShading && perSampleShading)
            {
                [unroll]
                for (uint sample = 0; sample < MSAA_SAMPLES; ++sample)
                {
                    WriteSample(globalCoords, sample, float4(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }
            else
            {
                // 境界の可視化が無効、または非境界ピクセルの場合は通常のシェーディングを実行する
                float3 lit = float3(0.0f, 0.0f, 0.0f);
                
                //
                // AmbientLight
                //
                if (g_UseIBL)
                    lit += ComputeIBLAmbientLight(surfaceSamples[0]);
                
                // Direction Light
                lit += ComputeDirectionalLight(surfaceSamples[0])
                                                 * surfaceSamples[0].albedo.w;
                
                // PointLights
                for (uint tileLightIndex = 0; tileLightIndex < numLights; tileLightIndex++)
                {
                    lit += ComputePointLight(surfaceSamples[0], tileLightIndex);
                }
                
                // サンプル0の結果を計算し、フレームバッファに書き込む
                // 可視化が無効かつMSAAなしの場合、ここで処理は終了する
                lit *= surfaceSamples[0].ambientOcclusion;
                WriteSample(globalCoords, 0, float4(lit, 1.0f));
                
                [branch]
                if (perSampleShading)
                {
#if DEFER_PER_SAMPLE
                    // サンプル単位でシェーディングが必要なピクセルのリストを作成する
                    uint listIndex;
                    InterlockedAdd(gs_NumPerSamplePixels, 1, listIndex);
                    gs_PerSamplePixels[listIndex] = PackCoords(globalCoords);
#else
                    
                    // 現在のピクセルの他のサンプルに対してシェーディングを行う
                    for (uint sample = 1; sample < MSAA_SAMPLES; ++sample)
                    {
                        float3 litSample = float3(0.0f, 0.0f, 0.0f);
                    
                        if(g_UseIBL)
                            litSample += ComputeIBLAmbientLight(surfaceSamples[sample]);
                    
                        litSample += ComputeDirectionalLight(surfaceSamples[sample])    
                                                                    * surfaceSamples[sample].albedo.w;
                        
                        for (uint tileLightIndex = 0; tileLightIndex < numLights; ++tileLightIndex)
                        {
                            litSample += ComputePointLight(surfaceSamples[sample], tileLightIndex);
                            
                        }
                        litSample*=surfaceSamples[sample].ambientOcclusion;
                        WriteSample(globalCoords, sample, float4(litSample, 1.0f));
                    }
                    
#endif
                }
                else
                {
                    
                    // それ以外の場合はピクセル単位でシェーディングを行い
                    // サンプル0の結果を他のサンプルにもコピーする
                    [unroll]
                    for (uint sample = 1; sample < MSAA_SAMPLES; ++sample)
                    {
                           
                        WriteSample(globalCoords, sample, float4(lit, 1.0f));
                    }
                }
            }
        }
        else
        {
            // 点光源の影響を受けないタイルに対しては、ディレクショナルライトのみを計算する
            [unroll]
            for (uint sample = 0; sample < MSAA_SAMPLES; ++sample)
            { 
                float3 lit = ComputeDirectionalLight(surfaceSamples[sample])
                                                        * surfaceSamples[sample].albedo.w;
                if (g_UseIBL)
                    lit += ComputeIBLAmbientLight(surfaceSamples[sample]);
                
                if (g_VisualizeCascades)
                {
                    float4 visualizeCascadeColor =
                                GetCascadeColorMultipler(surfaceSamples[sample].cascadeIndex, surfaceSamples[sample].nextCascadeIndex, saturate(surfaceSamples[sample].blendWeight));
                    WriteSample(globalCoords, sample, visualizeCascadeColor);
                }
                else
                {        
                    lit *= surfaceSamples[sample].ambientOcclusion;
                    WriteSample(globalCoords, sample, float4(lit, 1.0f));
                }
            }
        }
    }

    
    // これから逐サンプルシェーディングが必要なピクセルを処理する
#if DEFER_PER_SAMPLE && MSAA_SAMPLES > 1
    GroupMemoryBarrierWithGroupSync();
    
    // 注意：各ピクセルには追加で MSAA_SAMPLES - 1 回のシェーディングパスが必要
    const uint shadingPassesPerPixel = MSAA_SAMPLES - 1;
    // 総タスク数は、すべての境界にある MSAA ピクセルの数に相当する
    uint globalSamples = gs_NumPerSamplePixels * shadingPassesPerPixel;
   
    for (uint globalSample = groupIndex; globalSample < globalSamples; globalSample += COMPUTE_SHADER_TILE_GROUP_SIZE)
    {
        // 現在のタスクに対応する境界 MSAA ピクセルの共有メモリ内のインデックス
        uint listIndex = globalSample / shadingPassesPerPixel;
        // サンプル0はすでに処理済みなので、追加のサンプルを処理する
        uint sampleIndex = globalSample % shadingPassesPerPixel + 1;
        
        uint2 sampleCoords = UnpackCoords(gs_PerSamplePixels[listIndex]);
        SurfaceData surface = ComputeSurfaceDataFromGBufferSample(sampleCoords, sampleIndex);
        
        float3 lit = float3(0.0f, 0.0f, 0.0f);
        if(g_UseIBL)
            lit += ComputeIBLAmbientLight(surface);
        
        lit += ComputeDirectionalLight(surface) * surface.albedo.w;;
                     
        for (uint tileLightIndex = 0; tileLightIndex < numLights; ++tileLightIndex)
        {
            lit += ComputePointLight(surface, tileLightIndex);
        }
        // 追加のMSAAサンプルのみを処理する
        lit *= surface.ambientOcclusion;   
        WriteSample(sampleCoords, sampleIndex, float4(lit, 1.0f));
    }
#endif
}

#endif