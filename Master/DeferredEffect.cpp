#include "Effects.h"
#include <XUtil.h>
#include <RenderStates.h>
#include <EffectHelper.h>
#include <DXTrace.h>
#include <Vertex.h>
#include <TextureManager.h>
#include <iostream>
using namespace DirectX;

#pragma warning(disable: 26812)

//
// DeferredEffect::Impl は DeferredEffect の定義より先に
//
class DeferredEffect::Impl
{
public:
    // 明示的なコンストラクタ
    Impl() {}
    ~Impl() = default;

public:
    // COMポインタのエイリアス
    template<class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    std::unique_ptr<EffectHelper> m_pEffectHelper;
    // 現在使用中のEffectPass
    std::shared_ptr<IEffectPass> m_pCurrEffectPass;

    ComPtr<ID3D11InputLayout> m_pCurrInputLayout;
    D3D11_PRIMITIVE_TOPOLOGY m_Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ComPtr<ID3D11InputLayout> m_pVertexPosNormalTangentTexLayout;

    // Alpha Clipが有効かどうか
    bool m_AlphaClip = false;
    // ARM統合テクスチャとして持つかどうか
    bool m_HasRoughnessMetallicMap = false;
    // MSAAサンプル数
    UINT m_MsaaSamples = 1;
    // シャドウマップの種類（0:CSM, 1:VSM, 2:ESM, 3:EVSM2, 4:EVSM4）
    int m_ShadowType = 0;
    // 使用するカスケードレベル
    int m_CascadeLevel = 0;
    // カスケード選択方法
    int m_CascadeSelection = 0;
    // PCF時のKernelサイズ（奇数である必要がある）
    int m_PCFKernelSize = 1;
    // シャドウマップの解像度
    int m_ShadowSize = 1024;
    // ワールド/ビュー/プロジェクション行列
    XMFLOAT4X4 m_World{}, m_View{}, m_Proj{};
};


//
// DeferredEffect
//

namespace
{
    static DeferredEffect* g_pInstance = nullptr;
}

DeferredEffect::DeferredEffect()
{
    if (g_pInstance)
        throw std::exception("DeferredEffect is a singleton!");
    g_pInstance = this;
    pImpl = std::make_unique<DeferredEffect::Impl>();
}

DeferredEffect::~DeferredEffect()
{
}

DeferredEffect::DeferredEffect(DeferredEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
}

DeferredEffect& DeferredEffect::operator=(DeferredEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
    return *this;
}

DeferredEffect& DeferredEffect::Get()
{
    if (!g_pInstance)
        throw std::exception("DeferredEffect needs an instance!");
    return *g_pInstance;
}

bool DeferredEffect::InitAll(ID3D11Device* device)
{
    if (!device)
        return false;

    if (!RenderStates::IsInit())
        throw std::exception("RenderStates need to be initialized first!");

    pImpl->m_pEffectHelper = std::make_unique<EffectHelper>();

    pImpl->m_pEffectHelper->SetBinaryCacheDirectory(L"Shaders\\Cache");

    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    // Enable 
    // [0] -> MSAA
    // [1] -> MIXED_RM
    D3D_SHADER_MACRO defines[3] = {};

    D3D_SHADER_MACRO definesCS[] =
    {
        { "SHADOW_TYPE", "0" },
        { "CASCADE_COUNT_FLAG", "1" },
        { "SELECT_CASCADE_BY_INTERVAL_FLAG", "0" },
        { "USE_MIXED_ARM_MAP", "" },
        { "MSAA_SAMPLES", "1" },
        { nullptr,nullptr}
    };

    // ******************
    // 頂点シェーダを作成する
    //
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("FullScreenTriangleVS", L"Shaders\\FullScreenTriangle.hlsl",
        device, "FullScreenTriangleVS", "vs_5_0"));
    defines[0].Name = "MSAA_SAMPLES";
    defines[0].Definition = "1";
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("GeometryVS", L"Shaders\\GBuffer.hlsl",
        device, "GeometryVS", "vs_5_0", defines, blob.GetAddressOf()));
    // Create InputLayout
    HR(device->CreateInputLayout(VertexPosNormalTangentTex::GetInputLayout(), ARRAYSIZE(VertexPosNormalTangentTex::GetInputLayout()),
        blob->GetBufferPointer(), blob->GetBufferSize(), pImpl->m_pVertexPosNormalTangentTexLayout.ReleaseAndGetAddressOf()));


    // ******************
    // ピクセルシェーダ/コンピュートシェーダを作成する 
    //

    // MSAA level
    int msaaSamples = 1;
    while (msaaSamples <= 8)
    {
        std::string msaaSamplesStr = std::to_string(msaaSamples);
        defines[0].Definition = msaaSamplesStr.c_str();

        std::string shaderNames[] = {
            "GBufferMixedARM_" + msaaSamplesStr + "xMSAA_PS",
            "GBuffer_" + msaaSamplesStr + "xMSAA_PS",
            "RequiresPerSampleShading_" + msaaSamplesStr + "xMSAA_PS",
            "BasicDeferred_" + msaaSamplesStr + "xMSAA_PS",
            "BasicDeferredPerSample_" + msaaSamplesStr + "xMSAA_PS",
            "000_ComputeShaderTileDeferred_" + msaaSamplesStr + "xMSAA_CS"
        };

        std::string debugShaderNames[] = {
            "DebugNormal_" + msaaSamplesStr + "xMSAA_PS",
            "DebugAlbedo_" + msaaSamplesStr + "xMSAA_PS",
            "DebugMetallic_" + msaaSamplesStr + "xMSAA_PS",
            "DebugRoughness_" + msaaSamplesStr + "xMSAA_PS",
            "DebugPosZGrad_" + msaaSamplesStr + "xMSAA_PS",
        };

        const char* numStrs[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8" };

        defines[1].Name = "USE_MIXED_ARM_MAP";
        defines[1].Definition = "";
        // GBufferMixedARM_1xMSAA_PS 
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[0], L"Shaders\\GBuffer.hlsl",
            device, "GBufferPS", "ps_5_0", defines));
        
        defines[1].Name = nullptr;
        defines[1].Definition = nullptr;
        // GBuffer_1xMSAA_PS 
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[1], L"Shaders\\GBuffer.hlsl",
            device, "GBufferPS", "ps_5_0", defines));

        // RequiresPerSampleShading_1xMSAA_PS
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[2], L"Shaders\\GBuffer.hlsl",
            device, "RequiresPerSampleShadingPS", "ps_5_0", defines));

        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(debugShaderNames[0], L"Shaders\\GBuffer.hlsl",
            device, "DebugNormalPS", "ps_5_0", defines));
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(debugShaderNames[1], L"Shaders\\GBuffer.hlsl",
            device, "DebugAlbedoPS", "ps_5_0", defines));
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(debugShaderNames[2], L"Shaders\\GBuffer.hlsl",
            device, "DebugMetallicPS", "ps_5_0", defines));
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(debugShaderNames[3], L"Shaders\\GBuffer.hlsl",
            device, "DebugRoughnessPS", "ps_5_0", defines));
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(debugShaderNames[4], L"Shaders\\GBuffer.hlsl",
            device, "DebugPosZGradPS", "ps_5_0", defines));

        // ******************
        // Create Passes
        //
        EffectPassDesc passDesc;
        std::string passNames[] = {
            "GBufferMixedARM_" + msaaSamplesStr + "xMSAA",
            "GBuffer_" + msaaSamplesStr + "xMSAA",
            "Lighting_Basic_MaskStencil_" + msaaSamplesStr + "xMSAA",
            "Lighting_Basic_Deferred_PerPixel_" + msaaSamplesStr + "xMSAA",
            "Lighting_Basic_Deferred_PerSample_" + msaaSamplesStr + "xMSAA",
            "000_ComputeShaderTileDeferred_" + msaaSamplesStr + "xMSAA"
        };

        std::string debugPassNames[] = {
            "DebugNormal_" + msaaSamplesStr + "xMSAA",
            "DebugAlbedo_" + msaaSamplesStr + "xMSAA",
            "DebugMetallic_" + msaaSamplesStr + "xMSAA",
            "DebugRoughness_" + msaaSamplesStr + "xMSAA",
            "DebugPosZGrad_" + msaaSamplesStr + "xMSAA",
        };

        // All MSAA Levels
        definesCS[4].Definition = msaaSamplesStr.c_str();
        // All ShadowTypes
        for (int shadowType = 0; shadowType < 5; ++shadowType)
        {
            shaderNames[5][0] = passNames[5][0] = '0' + shadowType;
            definesCS[0].Definition = numStrs[shadowType];
            // All Cascades
            for (int cascadeCount = 1; cascadeCount <= 4; ++cascadeCount)
            {
                shaderNames[5][1] = passNames[5][1] = '0' + cascadeCount;
                definesCS[1].Definition = numStrs[cascadeCount];
                // CASCADE Flag
                for (int intervalIdx = 0; intervalIdx < 2; ++intervalIdx)
                {
                    shaderNames[5][2] = passNames[5][2] = '0' + intervalIdx;
                    definesCS[2].Definition = numStrs[intervalIdx];
           
                    // 000_ComputeShaderTileDeferred_1xMSAA_PS
                    HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[5], L"Shaders\\ComputeShaderTile.hlsl",
                        device, "ComputeShaderTileDeferredCS", "cs_5_0", definesCS));
                    // 000_ComputeShaderTileDeferred_1xMSAA
                    passDesc.nameVS = "";
                    passDesc.namePS = "";
                    passDesc.nameCS = shaderNames[5].c_str();
                    HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[5], device, &passDesc));
                    passDesc.nameCS = "";
                }
            }
        }

        passDesc.nameVS = "GeometryVS";
        passDesc.namePS = shaderNames[0].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[0], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[0]);
            // Reversed-Z => GREATER_EQUALテスト
            pPass->SetDepthStencilState(RenderStates::DSSGreaterEqual.Get(), 0);
        }

        passDesc.nameVS = "GeometryVS";
        passDesc.namePS = shaderNames[1].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[1], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[1]);
            // Reversed-Z => GREATER_EQUALテスト
            pPass->SetDepthStencilState(RenderStates::DSSGreaterEqual.Get(), 0);
        }

        passDesc.nameVS = "FullScreenTriangleVS";
        passDesc.namePS = shaderNames[2].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[2], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[2]);
            pPass->SetDepthStencilState(RenderStates::DSSWriteStencil.Get(), 1);
        }
      

        passDesc.nameVS = "FullScreenTriangleVS";
        passDesc.namePS = debugShaderNames[0].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(debugPassNames[0], device, &passDesc));

        passDesc.namePS = debugShaderNames[1].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(debugPassNames[1], device, &passDesc));

        passDesc.namePS = debugShaderNames[2].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(debugPassNames[2], device, &passDesc));

        passDesc.namePS = debugShaderNames[3].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(debugPassNames[3], device, &passDesc));

        passDesc.namePS = debugShaderNames[4].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(debugPassNames[4], device, &passDesc));

        msaaSamples <<= 1;
    }
    
    // ******************
    // サンプラーを作成する
    //

    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamPointClamp", RenderStates::SSPointClamp.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamLinearClamp", RenderStates::SSLinearClamp.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamLinearWrap", RenderStates::SSLinearWrap.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamShadowCmp", RenderStates::SSShadowPCF.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamShadow", RenderStates::SSAnistropicClamp16x.Get());

    // Debug
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    SetDebugObjectName(pImpl->m_pVertexPosNormalTangentTexLayout.Get(), "DeferredEffect.VertexPosNormalTangentTexLayout");
    pImpl->m_pEffectHelper->SetDebugObjectName("DeferredEffect");
#endif

    return true;
}

void XM_CALLCONV DeferredEffect::SetWorldMatrix(DirectX::FXMMATRIX W)
{
    XMStoreFloat4x4(&pImpl->m_World, W);
}

void XM_CALLCONV DeferredEffect::SetViewMatrix(DirectX::FXMMATRIX V)
{
    XMStoreFloat4x4(&pImpl->m_View, V);
}

void XM_CALLCONV DeferredEffect::SetProjMatrix(DirectX::FXMMATRIX P)
{
    XMStoreFloat4x4(&pImpl->m_Proj, P);
}


void DeferredEffect::SetMaterial(const Material& material)
{
    TextureManager& tm = TextureManager::Get();
    static const XMFLOAT4 max_albedo = { 0.8f, 0.8f, 0.8f, 1.0f };

    auto pStr = material.TryGet<std::string>("$Albedo");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_AlbedoMap", pStr ? tm.GetTexture(*pStr) : tm.GetTexture("$Null"));
    auto pVec4 = material.TryGet<XMFLOAT4>("$Albedo");
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_kAlbedo")->SetFloatVector(4, reinterpret_cast<const float*>(pVec4 ? pVec4 : &max_albedo));

    pStr = material.TryGet<std::string>("$Normal");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_NormalMap", pStr ? tm.GetTexture(*pStr) : tm.GetTexture("$Normal"));
    
    auto pStrR = material.TryGet<std::string>("$Roughness");
    auto pStrM = material.TryGet<std::string>("$Metallic");
    if (pStrR && pStrM)
    {
        if (*pStrR == *pStrM)
        {
            pImpl->m_HasRoughnessMetallicMap = true;
            pImpl->m_pEffectHelper->SetShaderResourceByName("g_RoughnessMetallicMap", tm.GetTexture(*pStrR));
            pImpl->m_pEffectHelper->SetShaderResourceByName("g_RoughnessMap", nullptr);
            pImpl->m_pEffectHelper->SetShaderResourceByName("g_MetallicMap", nullptr);
        }
        else
        {
            pImpl->m_HasRoughnessMetallicMap = false;
            pImpl->m_pEffectHelper->SetShaderResourceByName("g_RoughnessMetallicMap", nullptr);
            pImpl->m_pEffectHelper->SetShaderResourceByName("g_RoughnessMap", pStrR ? tm.GetTexture(*pStrR) : tm.GetTexture("$Ones"));
            pImpl->m_pEffectHelper->SetShaderResourceByName("g_MetallicMap", pStrM ? tm.GetTexture(*pStrM) : tm.GetTexture("$Zeros"));
        }
    }
    else
    {
        pImpl->m_pEffectHelper->SetShaderResourceByName("g_RoughnessMetallicMap", tm.GetTexture("$Ones"));
    }

    auto pValue = material.TryGet<float>("$Roughness");
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_kRoughness")->SetFloat( 1.0f);
    pValue = material.TryGet<float>("$Metallic");
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_kMetallic")->SetFloat( 1.0f);
}

void DeferredEffect::SetCameraNearFar(float nearZ, float farZ)
{
    float nearFar[4] = { nearZ, farZ };
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_CameraNearFar")->SetFloatVector(4, nearFar);
}

void DeferredEffect::SetUseIBL(bool enable)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_UseIBL")->SetUInt(enable);
}

void DeferredEffect::SetCascadeOffsets(const DirectX::XMFLOAT4 offsets[4])
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_CascadeOffset")->SetRaw(offsets);
}

void DeferredEffect::SetCascadeScales(const DirectX::XMFLOAT4 scales[4])
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_CascadeScale")->SetRaw(scales);
}

void DeferredEffect::SetCascadeFrustumsEyeSpaceDepths(const float depths[8])
{
    float depthsArray[8][4] = { {depths[0]},{depths[1]}, {depths[2]}, {depths[3]},
        {depths[4]}, {depths[5]}, {depths[6]}, {depths[7]} };
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_CascadeFrustumsEyeSpaceDepthsFloat")->SetRaw(depths);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_CascadeFrustumsEyeSpaceDepthsFloat4")->SetRaw(depthsArray);
}

void XM_CALLCONV DeferredEffect::SetShadowViewMatrix(DirectX::FXMMATRIX ShadowView)
{
    XMMATRIX ShadowViewT = XMMatrixTranspose(ShadowView);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_ShadowView")->SetFloatMatrix(4, 4, (const float*)&ShadowViewT);
}


void DeferredEffect::SetCascadeBlendArea(float blendArea)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_CascadeBlendArea")->SetFloat(blendArea);
}

void DeferredEffect::SetCascadeVisulization(bool enable)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_VisualizeCascades")->SetSInt(enable);
}

MeshDataInput DeferredEffect::GetInputData(const MeshData& meshData)
{
    MeshDataInput input;

    input.pInputLayout = pImpl->m_pCurrInputLayout.Get();
    input.topology = pImpl->m_Topology;

    input.pVertexBuffers = {
        meshData.m_pVertices.Get(),
        meshData.m_pNormals.Get(),
        meshData.m_pTangents.Get(),
        meshData.m_pTexcoordArrays.empty() ? nullptr : meshData.m_pTexcoordArrays[0].Get()
    };
    input.strides = { 12, 12, 16, 8 };
    input.offsets = { 0, 0, 0, 0 };

    input.pIndexBuffer = meshData.m_pIndices.Get();
    input.indexCount = meshData.m_IndexCount;

    return input;
}

void DeferredEffect::SetDirectionalLight(
    const DirectX::XMFLOAT3& lightDir,
    const DirectX::XMFLOAT3& lightColor,
    float lightIntensity)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_DirLightDir")->SetFloatVector(3, (FLOAT*)&lightDir);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_DirLightColor")->SetFloatVector(3, (FLOAT*)&lightColor);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_DirLightIntensity")->SetFloat(lightIntensity);
}

void DeferredEffect::SetMsaaSamples(UINT msaaSamples)
{
    pImpl->m_MsaaSamples = msaaSamples;
}

void DeferredEffect::SetVisualizeShadingFreq(bool enable)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_VisualizePerSampleShading")->SetUInt(enable);
}

void DeferredEffect::SetVisualizeLightCount(bool enable)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_VisualizeLightCount")->SetUInt(enable);
}


void DeferredEffect::SetShadowTextureArray(ID3D11ShaderResourceView* shadow)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_ShadowMap", shadow);
}

void DeferredEffect::SetPCFDepthBias(float offset)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_PCFDepthBias")->SetFloat(offset);
}

void DeferredEffect::SetPCFKernelSize(int size)
{
    // PCF内核のサイズに応じたループ範囲を設定
    int start = -size / 2;
    int end = size + start;
    pImpl->m_PCFKernelSize = size;
    float padding = (float)(size / 2) / (float)pImpl->m_ShadowSize;
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_PCFBlurForLoopStart")->SetSInt(start);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_PCFBlurForLoopEnd")->SetSInt(end);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_MinBorderPadding")->SetFloat(padding);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_MaxBorderPadding")->SetFloat(1.0f - padding);
}

void DeferredEffect::SetShadowSize(int size)
{
    pImpl->m_ShadowSize = size;
    float padding = 1.0f / (float)size;
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_TexelSize")->SetFloat(1.0f / size);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_MinBorderPadding")->SetFloat(padding);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_MaxBorderPadding")->SetFloat(1.0f - padding);
}

void DeferredEffect::SetCascadeLevels(int cascadeLevels)
{
    pImpl->m_CascadeLevel = cascadeLevels;
}

void DeferredEffect::SetCascadeIntervalSelectionEnabled(bool enable)
{
    pImpl->m_CascadeSelection = enable;
}

void DeferredEffect::SetShadowType(int type)
{
    if (type > 4 || type < 0)
        return;
    pImpl->m_ShadowType = type;
}

void DeferredEffect::SetLightBleedingReduction(float value)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_LightBleedingReduction")->SetFloat(value);
}

void DeferredEffect::SetMagicPower(float power)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_MagicPower")->SetFloat(power);
}

void DeferredEffect::SetPosExponent(float posExp)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_EvsmPosExp")->SetFloat(posExp);
}

void DeferredEffect::SetNegExponent(float negExp)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_EvsmNegExp")->SetFloat(negExp);
}

UINT DeferredEffect::GetMsaaSamples()
{
    return pImpl->m_MsaaSamples;
}

void DeferredEffect::ComputeTiledLightCulling(ID3D11DeviceContext* deviceContext, 
    ID3D11UnorderedAccessView* litFlatBufferUAV,
    ID3D11ShaderResourceView* pointLightBufferSRV, 
    ID3D11ShaderResourceView* GBuffers[4],
    ID3D11ShaderResourceView* ssaoTexture,
    ID3D11ShaderResourceView* shadowTextureArraySRV,
    ID3D11ShaderResourceView* irradianceMap,
    ID3D11ShaderResourceView* prefilterMap,
    ID3D11ShaderResourceView* BRDFLUT)
{
    // UAVを事前にクリアする必要はない（全ピクセルに書き込みを行うため）
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTex;
    GBuffers[0]->GetResource(reinterpret_cast<ID3D11Resource**>(pTex.GetAddressOf()));
    D3D11_TEXTURE2D_DESC texDesc;
    pTex->GetDesc(&texDesc);

    // フレームバッファの幅・高さを定数バッファへ送る
    UINT dims[2] = { texDesc.Width, texDesc.Height };
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_FramebufferDimensions")->SetUIntVector(2, dims);
    // GBuffer テクスチャの設定
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[0]", GBuffers[0]);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[1]", GBuffers[1]);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[2]", GBuffers[2]);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[3]", GBuffers[3]);
    // SSAO
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_SSAOTexture", ssaoTexture);
    // CSM
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_ShadowMap", shadowTextureArraySRV);
    // IBL
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_IBLIrradianceMap", irradianceMap);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_IBLPrefilterMap", prefilterMap);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_IBLBRDFLUT", BRDFLUT);
    // PointLights
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_PointLight", pointLightBufferSRV);
    pImpl->m_pEffectHelper->SetUnorderedAccessByName("g_Framebuffer", litFlatBufferUAV, 0);
    // Compute Pass の名前を構築（ShadowType、CascadeLevel、IntervalFlag に基づく）
    std::string passName = "000_ComputeShaderTileDeferred_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    passName[0] = '0' + pImpl->m_ShadowType;
    passName[1] = '0' + pImpl->m_CascadeLevel;
    passName[2] = '0' + pImpl->m_CascadeSelection;
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pPass->Apply(deviceContext);
    pPass->Dispatch(deviceContext, texDesc.Width, texDesc.Height);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_Framebuffer");
    litFlatBufferUAV = nullptr;
    deviceContext->CSSetUnorderedAccessViews(slot, 1, &litFlatBufferUAV, nullptr);

    slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_PointLight");
    pointLightBufferSRV = nullptr;
    deviceContext->CSSetShaderResources(slot, 1, &pointLightBufferSRV);

    ID3D11ShaderResourceView* nullSRVs[4] = {};
    slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_GBufferTextures[0]");
    deviceContext->CSSetShaderResources(slot, 4, nullSRVs);

    slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_ShadowMap");
    shadowTextureArraySRV = nullptr;
    deviceContext->CSSetShaderResources(slot, 1, &shadowTextureArraySRV);

    slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_SSAOTexture");
    ssaoTexture = nullptr;
    deviceContext->CSSetShaderResources(slot, 1, &ssaoTexture);

    //slot= pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_IBLIrradianceMap");
    //irradianceMap = nullptr;
    //deviceContext->CSSetShaderResources(slot, 1, &irradianceMap);

    //slot = pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_IBLPrefilterMap");
    //prefilterMap = nullptr;
    //deviceContext->CSSetShaderResources(slot, 1, &prefilterMap);

    //slot = pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_IBLBRDFLUT");
    //BRDFLUT = nullptr;
    //deviceContext->CSSetShaderResources(slot, 1, &BRDFLUT);
}



void DeferredEffect::DebugNormalGBuffer(ID3D11DeviceContext* deviceContext,
    ID3D11RenderTargetView* rtv,
    ID3D11ShaderResourceView* normalGBuffer,
    D3D11_VIEWPORT viewport)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext->RSSetViewports(1, &viewport);

    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[0]", normalGBuffer);
    std::string passStr = "DebugNormal_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passStr);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &rtv, nullptr);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_GBufferTextures[0]");
    normalGBuffer = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &normalGBuffer);
}



void DeferredEffect::DebugAlbedoGBuffer(ID3D11DeviceContext* deviceContext,
    ID3D11RenderTargetView* rtv,
    ID3D11ShaderResourceView* albedoGBuffer,
    D3D11_VIEWPORT viewport)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext->RSSetViewports(1, &viewport);

    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[1]", albedoGBuffer);
    std::string passStr = "DebugAlbedo_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passStr);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &rtv, nullptr);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_GBufferTextures[1]");
    albedoGBuffer = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &albedoGBuffer);
}

void DeferredEffect::DebugPosZGradGBuffer(ID3D11DeviceContext* deviceContext, 
    ID3D11RenderTargetView* rtv, 
    ID3D11ShaderResourceView* posZGradGBuffer, 
    D3D11_VIEWPORT viewport)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext->RSSetViewports(1, &viewport);

    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[2]", posZGradGBuffer);
    std::string passStr = "DebugPosZGrad_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passStr);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &rtv, nullptr);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_GBufferTextures[2]");
    posZGradGBuffer = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &posZGradGBuffer);
}

void DeferredEffect::DebugRoughnessGBuffer(ID3D11DeviceContext* deviceContext,
    ID3D11RenderTargetView* rtv,
    ID3D11ShaderResourceView* roughnessGBuffer,
    D3D11_VIEWPORT viewport)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext->RSSetViewports(1, &viewport);

    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[0]", roughnessGBuffer);
    std::string passStr = "DebugRoughness_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passStr);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &rtv, nullptr);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_GBufferTextures[0]");
    roughnessGBuffer = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &roughnessGBuffer);
}

void DeferredEffect::DebugMetallicGBuffer(ID3D11DeviceContext* deviceContext,
    ID3D11RenderTargetView* rtv,
    ID3D11ShaderResourceView* metallicGBuffer,
    D3D11_VIEWPORT viewport)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext->RSSetViewports(1, &viewport);

    pImpl->m_pEffectHelper->SetShaderResourceByName("g_GBufferTextures[0]", metallicGBuffer);
    std::string passStr = "DebugMetallic_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passStr);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &rtv, nullptr);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_GBufferTextures[0]");
    metallicGBuffer = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &metallicGBuffer);
}

void DeferredEffect::Apply(ID3D11DeviceContext* deviceContext)
{
    XMMATRIX W = XMLoadFloat4x4(&pImpl->m_World);
    XMMATRIX V = XMLoadFloat4x4(&pImpl->m_View);
    XMMATRIX P = XMLoadFloat4x4(&pImpl->m_Proj);

    XMMATRIX WV = W * V;
    XMMATRIX WVP = WV * P;
    XMMATRIX InvV = XMMatrixInverse(nullptr, V);
    XMMATRIX WInvTV = XMath::InverseTranspose(W) * V;

    WV = XMMatrixTranspose(WV);
    WVP = XMMatrixTranspose(WVP);
    InvV = XMMatrixTranspose(InvV);
    WInvTV = XMMatrixTranspose(WInvTV);
    P = XMMatrixTranspose(P);

    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldInvTransposeView")->SetFloatMatrix(4, 4, (FLOAT*)&WInvTV);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldViewProj")->SetFloatMatrix(4, 4, (FLOAT*)&WVP);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldView")->SetFloatMatrix(4, 4, (FLOAT*)&WV);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_InvView")->SetFloatMatrix(4, 4, (FLOAT*)&InvV);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_Proj")->SetFloatMatrix(4, 4, (FLOAT*)&P);


    if (pImpl->m_pCurrEffectPass)
        pImpl->m_pCurrEffectPass->Apply(deviceContext);    
}

void DeferredEffect::SetRenderGBuffer(bool alphaClip)
{
    std::string gBufferPassStr = "";
    if (pImpl->m_HasRoughnessMetallicMap)
        gBufferPassStr = "GBufferMixedARM_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";
    else
        gBufferPassStr = "GBuffer_" + std::to_string(pImpl->m_MsaaSamples) + "xMSAA";

    pImpl->m_AlphaClip = alphaClip;
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(gBufferPassStr);
    pImpl->m_pCurrEffectPass = pPass;
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTangentTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pPass->PSGetParamByName("alphaClip")->SetSInt(pImpl->m_AlphaClip);
    
}

