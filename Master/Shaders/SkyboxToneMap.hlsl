#ifndef SKYBOX_TONE_MAP_HLSL
#define SKYBOX_TONE_MAP_HLSL

#include "GBuffer.hlsl"
#include "FrameBufferFlat.hlsl"

#ifndef MSAA_SAMPLES
#define MSAA_SAMPLES 1
#endif

TextureCube<float4>                 g_SkyboxTexture :  register(t5);
Texture2DMS<float, MSAA_SAMPLES>    g_DepthTexture :   register(t6);
// 通常のマルチサンプルシーンレンダリング用のテクスチャ
Texture2DMS<float4, MSAA_SAMPLES>   g_LitTexture :     register(t7);
// コンピュートシェーダーではマルチサンプルのUAVに書き込めないため、1Dの連続配列で表現する
StructuredBuffer<uint2>             g_FlatLitTexture : register(t8);

struct SkyboxVSOut
{
    float4 posViewport : SV_Position;
    float3 skyboxCoord : skyboxCoord;
};



float3 TM_Reinhard(float3 hdr, float k)
{
    return hdr / (hdr + k);
}

float3 TM_Standard(float3 hdr)
{
    return TM_Reinhard(hdr * sqrt(hdr), sqrt(4.0f / 27.0f));
}

float3 TM_ACES_Coarse(float3 hdr)
{
    static const float a = 2.51f;
    static const float b = 0.03f;
    static const float c = 2.43f;
    static const float d = 0.59f;
    static const float e = 0.14f;
    return saturate((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e));
}

float3 TM_ACES(float3 hdr)
{
    static float3x3 aces_input_mat = float3x3(
        0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f);
    
    static float3x3 aces_output_mat = float3x3(
        1.60475f, -0.53108f, -0.07367f,
        -0.10208f, 1.10813f, -0.00605f,
        -0.00327f, -0.07276f, 1.07602f);
    
    float3 res = mul(aces_input_mat, hdr);
    float3 a = res * (res + 0.0245786f) - 0.000090537f;
    float3 b = res * (0.983729f * res + 0.4329510f) + 0.238081f;
    return mul(aces_output_mat, a / b);
}

SkyboxVSOut SkyboxToneMapVS(VertexPosNormalTex input)
{
    SkyboxVSOut output;
    
    // 注意：スカイボックスを移動させず、深度値が1になるようにする（クリッピングを防ぐため）
    output.posViewport = mul(float4(input.posL, 0.0f), g_ViewProj).xyww;
    output.skyboxCoord = input.posL;

    return output;
}

float4 SkyboxToneMapCubePS(SkyboxVSOut input) : SV_Target
{
    // 1D の MSAA シーンレンダーターゲットが提供されている場合、それを利用する
    uint2 dims;
    g_FlatLitTexture.GetDimensions(dims.x, dims.y);
    bool useFlatLitBuffer = dims.x > 0;
    
    uint2 coords = input.posViewport.xy;

    float3 lit = float3(0.0f, 0.0f, 0.0f);
    float skyboxSamples = 0.0f;
#if MSAA_SAMPLES <= 1
    [unroll]
#endif
    for (uint sampleIndex = 0; sampleIndex < MSAA_SAMPLES; ++sampleIndex)
    {
        float depth = g_DepthTexture.Load(coords, sampleIndex);
        
        // スカイボックスの状態を確認する（注意：リバースZ！）
        if (depth <= 0.0f && !g_VisualizeLightCount)
        {
            ++skyboxSamples;
        }
        else
        {
            float3 sampleLit;
            [branch]
            if (useFlatLitBuffer)
            {
                sampleLit = UnpackRGBA16(g_FlatLitTexture[GetFramebufferSampleAddress(coords, sampleIndex)]).xyz;
            }
            else
            {
                uint texWidth, texHeight, dummy;
                g_LitTexture.GetDimensions(texWidth, texHeight, dummy);
                sampleLit = g_LitTexture.Load(input.posViewport.xy / float2(texWidth, texHeight), sampleIndex);
            }
            
            lit += sampleLit;
        }
        
    }
    
    // ここでシーンが描画されていない場合は、スカイボックスを描画する
    [branch]
    if (skyboxSamples > 0)
    {
        float3 skybox = g_SkyboxTexture.Sample(g_SamAnistropicWrap16x, input.skyboxCoord).xyz;
        lit += skyboxSamples * skybox;
    }
    
    lit *= rcp((float) MSAA_SAMPLES);
  
    // Tone Mapping
#if defined(TONEMAP_ACES)
    return pow(float4(TM_ACES(lit), 1.0f),2.2f);
#elif defined(TONEMAP_ACES_COARSE)
    return pow(float4(TM_ACES_Coarse(lit), 1.0f),2.2f);
#elif defined(TONEMAP_STANDARD)
    return pow(float4(TM_Standard(lit), 1.0f),2.2f);
#else
    return pow(float4(TM_Reinhard(lit, 1.0f), 1.0f), 2.2f);
#endif
    
}

#endif