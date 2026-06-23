#ifndef SSAO_HLSL
#define SSAO_HLSL

#define N 256
#define MAX_BLUR_RADIUS 5
#define CacheSize (N + 2 * MAX_BLUR_RADIUS) 

#define ScreenWidth  16
#define ScreenHeight  9

#ifndef MSAA_SAMPLES
#define MSAA_SAMPLES 1
#endif

#include "FullScreenTriangle.hlsl"

Texture2D g_DiffuseMap :    register(t0);
Texture2D g_RandomVecMap :  register(t1);

Texture2D g_NormalGBuffer : register(t2);
Texture2D g_DepthBuffer :   register(t3);

Texture2D g_InputImage :    register(t4);

RWTexture2D<float4> g_BlurOutput :  register(u0);
RWTexture2D<float> g_SSAOOutput :   register(u1);

groupshared float4 g_SSAOCache[CacheSize];
groupshared struct EdgeData
{
    float3 normal;
    float depth;
} g_EdgeCache[CacheSize];

cbuffer CBChangesEveryObjectDrawing : register(b0)
{
    matrix g_WorldView;
    matrix g_WorldViewProj;
    matrix g_WorldInvTransposeView;
    matrix g_InvProj;
    matrix g_Proj;
}

cbuffer CBChangesOnResize : register(b2)
{

    matrix g_ViewToTexSpace;            // Proj * Texture
    float4 g_FarPlanePoints[3];         // 远平面三角形(覆盖四个角点)，三角形见下
    float4 g_FarPlaneSquarePoints[4];
   
    float2  g_TexelSize;                // (1.0f / W, 1.0f / H)
    uint    g_SSAOX;
    uint    g_SSAOY;
}

cbuffer CBChangesRarely : register(b3)
{
    // 14方向に均等に分布したランダム長のベクトル
    float4 g_OffsetVectors[14];
    
    // View Space
    float g_OcclusionRadius;
    float g_OcclusionFadeStart;
    float g_OcclusionFadeEnd;
    float g_SurfaceEpsilon;
    
    //
    // SSAO_Blur
    //
    float4 g_BlurWeights[3];
    
    static float s_BlurWeights[12] = (float[12]) g_BlurWeights;
    
    int     g_BlurRadius;
    float3  g_PadBlurRadius;
}

SamplerState g_SamLinearWrap :  register(s0);
SamplerState g_SamNormalDepth : register(s1);
SamplerState g_SamRandomVec :   register(s2);

//
// Pass1 ジオメトリバッファ生成
//


struct VertexPosNormalTex
{
    float3 posL :       POSITION;
    float3 normalL :    NORMAL;
    float2 texCoord :   TEXCOORD;
};

struct VertexPosHVNormalVTex
{
    float4 posH :       SV_Position;
    float3 posV :       Position;
    float3 normalV :    NORMAL;
    float2 texCoord :   TEXCOORD;
};

// 法線デコード
float3 DecodeSphereMap(float2 encoded)
{
    float4 nn = float4(encoded, 1, -1);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return nn.xyz * 2 + float3(0, 0, -1);
}

// ZとNDC座標からposVを再構築
float3 ComputePositionViewFromZ(float2 posNdc, float viewSpaceZ)
{
    float2 screenSpaceRay = float2(posNdc.x / g_Proj._m00,
                                   posNdc.y / g_Proj._m11);
    
    float3 posV;
    posV.z = viewSpaceZ;
    posV.xy = screenSpaceRay.xy * posV.z;
    
    return posV;
}


// 法線 + 深度出力用のVS
VertexPosHVNormalVTex GeometryVS(VertexPosNormalTex input)
{
    VertexPosHVNormalVTex output;
    
    output.posH = mul(float4(input.posL, 1.0f), g_WorldViewProj);
    output.posV = mul(float4(input.posL, 1.0f), g_WorldView).xyz;
    output.normalV = mul(float4(input.normalL, 0.0f), g_WorldInvTransposeView).xyz;
    output.texCoord = input.texCoord;
    
    return output;
}




//
// Pass2 AO計算
//

// 点rと点pのZ距離から遮蔽率を計算
float OcclusionFunction(float distZ)
{
    //
    // depth(q) が depth(p) よりも遠い場合（半球の外側）、点 q は点 p を遮蔽できない。
    // また、depth(q) と depth(p) が近すぎる場合も、点 q は点 p を遮蔽しないとみなす。
    // なぜなら、depth(p) - depth(r) がユーザー定義の Epsilon を超えない限り、
    // 点 q は十分な遮蔽効果を持つとは判断できないため
    //
    // 以下の関数を使用して遮蔽の度合いを決定する
    //
    //    /|\ Occlusion
    // 1.0 |      ---------------\
    //     |      |             |  \
    //     |                         \
    //     |      |             |      \
    //     |                             \
    //     |      |             |          \
    //     |                                 \
    // ----|------|-------------|-------------|-------> zv
    //     0     Eps          zStart         zEnd
    float occlusion = 0.0f;
    if (distZ > g_SurfaceEpsilon)
    {
        float fadeLength = g_OcclusionFadeEnd - g_OcclusionFadeStart;
        // distZ が g_OcclusionFadeStart から g_OcclusionFadeEnd に近づくにつれて、
        // 遮蔽値は 1 から 0 へ線形に減衰する
        occlusion = saturate((g_OcclusionFadeEnd - distZ) / fadeLength);
    }
    
    return occlusion;
}
// 遠平面上の座標を補間
float3 InterpolateFarPlanePoint(float3 fp0, float3 fp1, float3 fp2, float3 fp3,
                                float2 texcoord)
{
    float3 interFarPlanePoint;
    
    //interFarPlanePoint = (1 - texcoord.x) * (1 - texcoord.y) * fp0 +
    //                        texcoord.x * (1 - texcoord.y) * fp1 +
    //                        (1 - texcoord.x) * texcoord.y * fp2 +
    //                        texcoord.x * texcoord.y * fp3;
    
    interFarPlanePoint = lerp(
                            lerp(fp0, fp1, texcoord.x),
                            lerp(fp2, fp3, texcoord.x),
                            texcoord.y);
    
    return interFarPlanePoint;
}


[numthreads(ScreenWidth, ScreenHeight, 1)]
void SSAO_CS(int3 GTid : SV_GroupThreadID,
             int3 DTid : SV_DispatchThreadID,
             uniform int sampleCount)
{
    // ピクセル中心を考慮してテクスチャ座標を算出
    float2 texcoord = (DTid.xy + 0.5f) / float2(g_SSAOX, g_SSAOY);
    
    // 遠平面上の補間された視点空間座標を取得（復元）
    float3 farPlanePoint = InterpolateFarPlanePoint(g_FarPlaneSquarePoints[0].xyz, g_FarPlaneSquarePoints[1].xyz,
        g_FarPlaneSquarePoints[2].xyz, g_FarPlaneSquarePoints[3].xyz, texcoord);
    
    // 法線をデコードして視点空間の法線ベクトルを得る
    float4 normalGBuffer = g_NormalGBuffer.SampleLevel(g_SamNormalDepth, texcoord, 0.0f);
    float3 n = DecodeSphereMap(normalGBuffer.xy);

    // 深度をデコードして視点空間のZ値に変換
    float zBuffer = g_DepthBuffer.SampleLevel(g_SamNormalDepth, texcoord, 0.0f).x;
    float pz = g_Proj._m32 / (zBuffer - g_Proj._m22);
    
    // ピクセル p の視点空間位置を再構築
    float3 p = (pz / farPlanePoint.z) * farPlanePoint;
    
    float2 gbufferDim;
    // テクスチャサイズ（ピクセル数）を取得
    g_DepthBuffer.GetDimensions(gbufferDim.x, gbufferDim.y);
    
    // NDC座標系のピクセルオフセットを計算（ピクセル中心は +0.5）
    float2 screenPixelOffset = float2(2.0f, -2.0f) / gbufferDim;
    float2 posNdc = (float2(DTid.xy) + 0.5f) * screenPixelOffset.xy + float2(-1.0f, 1.0f);
    
    
    // p -- SSAOを計算したい対象ピクセルの視点空間座標
    // n -- 点pの視点空間法線
    // q -- 半球内のサンプリング点（ランダム方向）
    // r -- 点qに対応する深度位置の視点空間座標
    
    // ランダムベクトルを[-1, 1]に変換し半球サンプリングをブレンド
    float3 randVec = g_RandomVecMap.SampleLevel(g_SamRandomVec, 4.0f * texcoord, 0.0f).xyz;
    randVec = 2.0f * randVec - 1.0f;

    float occlusionSum = 0.0f;
    
    // 点pの周囲に sampleCount 回のサンプリングを行い、遮蔽量を累積
    for (int i = 0; i < sampleCount; ++i)
    {
        // ランダム方向ベクトルから offset を作成
        float3 offset = reflect(g_OffsetVectors[i].xyz, randVec);
        
        // 法線との向きを基に半球の向こう側にいる場合は反転
        float flip = sign(dot(offset, n));
        
        // 半球内のサンプリング点 q を計算
        float3 q = p + flip * g_OcclusionRadius * offset;
        
        // 視点空間座標を射影してテクスチャ座標へ変換
        float4 projQ = mul(float4(q, 1.0f), g_ViewToTexSpace);
        projQ /= projQ.w;
        
        // 点 q に対応する深度値から視点空間の r を復元
        float rz = g_DepthBuffer.SampleLevel(g_SamNormalDepth, projQ.xy, 0.0f);
        rz = g_Proj._m32 / (rz - g_Proj._m22);
        float3 r = (rz / q.z) * q;
        
        // 遮蔽テスト：
        // - dot(n, normalize(r - p)) は遮蔽方向が法線方向に近いほど大きくなる（自遮蔽の影響を低減）
        // - p.z - r.z の差が g_SurfaceEpsilon を超える場合のみ遮蔽とみなす
        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        float occlusion = dp * OcclusionFunction(distZ);
        
        occlusionSum += occlusion;
    }

    // 遮蔽の平均値を取り、アクセシビリティとして出力（4乗でコントラスト強調）
    occlusionSum /= sampleCount;
    float access = 1.0f - occlusionSum;
    
    g_SSAOOutput[DTid.xy] = saturate(pow(access, 4.0f));
}

//
// Pass3 Blur AO
//

// 水平方向
[numthreads(N, 1, 1)]
void Blur_Horz_CS(int3 GTid : SV_GroupThreadID,
                  int3 DTid : SV_DispatchThreadID)
{
    // AOとエッジ用のテクスチャサイズを取得
    uint aoWidth, aoHeight;
    g_InputImage.GetDimensions(aoWidth, aoHeight);
    uint edgeWidth, edgeHeight;
    g_NormalGBuffer.GetDimensions(edgeWidth, edgeHeight);
    
    // スケーリング係数
    static int2 scaleFactor = int2(2, 2);
    
    // BlurRadiusに応じて共有メモリに必要な追加ピクセル数を読み込み
    if (GTid.x < g_BlurRadius)
    {
        // 左端でのクランプ処理
        int x = max(DTid.x - g_BlurRadius, 0);
        g_SSAOCache[GTid.x] = g_InputImage[int2(x, DTid.y)];
        
        EdgeData edgeData;
        edgeData.normal = DecodeSphereMap(g_NormalGBuffer[int2(x, DTid.y) * scaleFactor].xy);
        edgeData.depth = g_DepthBuffer[int2(x, DTid.y) * scaleFactor];
        g_EdgeCache[GTid.x] = edgeData;
        
    }

    
    if (GTid.x >= N - g_BlurRadius)
    {
        // 右端でのクランプ処理
        int x = min(DTid.x + g_BlurRadius, aoWidth - 1);
        g_SSAOCache[GTid.x + 2 * g_BlurRadius] = g_InputImage[int2(x, DTid.y)];
        
        EdgeData edgeData;
        edgeData.normal = DecodeSphereMap(g_NormalGBuffer[int2(x, DTid.y) * scaleFactor].xy);
        edgeData.depth = g_DepthBuffer[int2(x, DTid.y) * scaleFactor];
        g_EdgeCache[GTid.x + 2 * g_BlurRadius] = edgeData;

    }
    
    // 中心ピクセルの値をキャッシュに保存
    g_SSAOCache[GTid.x + g_BlurRadius] = g_InputImage[min(DTid.xy, uint2(aoWidth - 1, aoHeight - 1))];
    
    EdgeData edgeData;
    edgeData.normal = DecodeSphereMap(g_NormalGBuffer[min(DTid.xy * scaleFactor, uint2(edgeWidth - 1, edgeHeight - 1))].xy);
    edgeData.depth = g_DepthBuffer[min(DTid.xy * scaleFactor, uint2(edgeWidth - 1, edgeHeight - 1))];
    g_EdgeCache[GTid.x + g_BlurRadius] = edgeData;
    
    // スレッドグループ間の同期を行う
    GroupMemoryBarrierWithGroupSync();
    
    // 中心ピクセルの重みと色を初期化
    float totalWeight = s_BlurWeights[g_BlurRadius];
    float4 blurColor = s_BlurWeights[g_BlurRadius] * g_SSAOCache[GTid.x + g_BlurRadius];
    
    EdgeData centerNorDep = g_EdgeCache[GTid.x + g_BlurRadius];
    float3 centerNormal = centerNorDep.normal;
    float centerDepth = centerNorDep.depth;

    // 左右の近傍ピクセルを走査
    for (int i = -g_BlurRadius; i <= g_BlurRadius; ++i)
    {
        // 中心ピクセルはすでに加算済み
        if (i == 0)
            continue;
        
        int k = GTid.x + g_BlurRadius + i;
        EdgeData neighborNorDep = g_EdgeCache[k];
        float3 neighborNormal = neighborNorDep.normal;
        float neighborDepth = neighborNorDep.depth;
        
        // エッジ保護
        if (dot(neighborNormal, centerNormal) >= 0.8f && abs(neighborDepth - centerDepth) <= 0.2f)
        {
            float weight = s_BlurWeights[i + g_BlurRadius];
            blurColor += weight * g_SSAOCache[k];
            totalWeight += weight;
        }
    }
    
    g_BlurOutput[DTid.xy] = blurColor / totalWeight;
}

// 垂直方向
[numthreads(1, N, 1)]
void Blur_Vert_CS(int3 GTid : SV_GroupThreadID,
                  int3 DTid : SV_DispatchThreadID)
{
    // AOとエッジ用のテクスチャサイズを取得
    uint aoWidth, aoHeight;
    g_InputImage.GetDimensions(aoWidth, aoHeight);
    uint edgeWidth, edgeHeight;
    g_NormalGBuffer.GetDimensions(edgeWidth, edgeHeight);
    
    // スケーリング係数
    static int2 scaleFactor = int2(2, 2);
    
    // BlurRadiusに応じて共有メモリに必要な追加ピクセル数を読み込み
    if (GTid.y < g_BlurRadius)
    {
        // 左端でのクランプ処理
        int y = max(DTid.y - g_BlurRadius, 0);
        g_SSAOCache[GTid.y] = g_InputImage[int2(DTid.x, y)];
        
        EdgeData edgeData;
        edgeData.normal = DecodeSphereMap(g_NormalGBuffer[int2(DTid.x, y) * scaleFactor].xy);
        edgeData.depth = g_DepthBuffer[int2(DTid.x, y) * scaleFactor];
        g_EdgeCache[GTid.y] = edgeData;
    }
    
    if (GTid.y >= N - g_BlurRadius)
    {
        // 右端でのクランプ処理
        int y = min(DTid.y + g_BlurRadius, aoHeight - 1);
        g_SSAOCache[GTid.y + 2 * g_BlurRadius] = g_InputImage[int2(DTid.x, y)];

        EdgeData edgeData;
        edgeData.normal = DecodeSphereMap(g_NormalGBuffer[int2(DTid.x, y) * scaleFactor].xy);
        edgeData.depth = g_DepthBuffer[int2(DTid.x, y) * scaleFactor];
        g_EdgeCache[GTid.y + 2 * g_BlurRadius] = edgeData;
    }
    
    // 中心ピクセルの値をキャッシュに保存
    g_SSAOCache[GTid.y + g_BlurRadius] = g_InputImage[min(DTid.xy, uint2(aoWidth - 1, aoHeight - 1))];
    EdgeData edgeData;
    edgeData.normal = DecodeSphereMap(g_NormalGBuffer[min(DTid.xy * scaleFactor, uint2(edgeWidth - 1, edgeHeight - 1))].xy);
    edgeData.depth = g_DepthBuffer[min(DTid.xy * scaleFactor, uint2(edgeWidth - 1, edgeHeight - 1))];
    g_EdgeCache[GTid.y + g_BlurRadius] = edgeData;
    
    // スレッドグループ間の同期を行う
    GroupMemoryBarrierWithGroupSync();
    
    // 中心ピクセルの重みと色を初期化
    float totalWeight = s_BlurWeights[g_BlurRadius];
    float4 blurColor = s_BlurWeights[g_BlurRadius] * g_SSAOCache[GTid.y + g_BlurRadius];
    EdgeData centerNorDep = g_EdgeCache[GTid.y + g_BlurRadius];
    float3 centerNormal = centerNorDep.normal;
    float centerDepth = centerNorDep.depth;
    
    // 左右の近傍ピクセルを走査
    for (int i = -g_BlurRadius; i <= g_BlurRadius; ++i)
    {
        if (i == 0)
            continue;
        
        int k = GTid.y + g_BlurRadius + i;
        
        EdgeData neighborNorDep = g_EdgeCache[k];
        float3 neighborNormal = neighborNorDep.normal;
        float neighborDepth = neighborNorDep.depth;
        
        if (dot(neighborNormal, centerNormal) >= 0.8f && abs(neighborDepth - centerDepth) <= 0.2f)
        {
            float weight = s_BlurWeights[i + g_BlurRadius];
            blurColor += weight * g_SSAOCache[k];
            totalWeight += weight;
        }
    }
    
    g_BlurOutput[DTid.xy] = blurColor / totalWeight;
}


float4 DebugAO_PS(float4 posH : SV_position,
                  float2 texCoord : TEXCOORD) : SV_Target
{
    float depth = g_DiffuseMap.Sample(g_SamLinearWrap, texCoord).r;
    return float4(depth.rrr, 1.0f);
}

#endif