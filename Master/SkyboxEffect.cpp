#include "Effects.h"
#include <EffectHelper.h>
#include <RenderStates.h>
#include <XUtil.h>
#include <DXTrace.h>
#include <Vertex.h>
#include <TextureManager.h>

using namespace DirectX;

class SkyboxToneMapEffect::Impl
{
public:
    // 明示的なコンストラクタ
    Impl() {
        XMStoreFloat4x4(&m_World, XMMatrixIdentity());
        XMStoreFloat4x4(&m_View, XMMatrixIdentity());
        XMStoreFloat4x4(&m_Proj, XMMatrixIdentity());
    }
    ~Impl() = default;
public:
    template<class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    std::unique_ptr<EffectHelper> m_pEffectHelper;
    std::shared_ptr<IEffectPass> m_pCurrEffectPass;
    ComPtr<ID3D11InputLayout> m_pCurrInputLayout;
    D3D11_PRIMITIVE_TOPOLOGY m_Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

    ComPtr<ID3D11InputLayout> m_pVertexPosNormalTexLayout;

    XMFLOAT4X4 m_World, m_View, m_Proj;
    UINT m_MsaaLevels = 1;
};

//
// SkyboxToneMapEffect
//

namespace
{
    static SkyboxToneMapEffect* g_pInstance = nullptr;
}

SkyboxToneMapEffect::SkyboxToneMapEffect()
{
    if (g_pInstance)
        throw std::exception("SkyboxToneMapEffect is a singleton!");
    g_pInstance = this;
    pImpl = std::make_unique<SkyboxToneMapEffect::Impl>();
}

SkyboxToneMapEffect::~SkyboxToneMapEffect()
{
}

SkyboxToneMapEffect::SkyboxToneMapEffect(SkyboxToneMapEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
}

SkyboxToneMapEffect& SkyboxToneMapEffect::operator=(SkyboxToneMapEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
    return *this;
}

SkyboxToneMapEffect& SkyboxToneMapEffect::Get()
{
    if (!g_pInstance)
        throw std::exception("SkyboxToneMapEffect needs an instance!");
    return *g_pInstance;
}

bool SkyboxToneMapEffect::InitAll(ID3D11Device* device)
{
    if (!device)
        return false;

    if (!RenderStates::IsInit())
        throw std::exception("RenderStates need to be initialized first!");

    pImpl->m_pEffectHelper = std::make_unique<EffectHelper>();

    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    pImpl->m_pEffectHelper->SetBinaryCacheDirectory(L"Shaders\\Cache\\");

    D3D_SHADER_MACRO defines[3] = { };

    // ***********************
    // 頂点シェーダを作成する
    //
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("SkyboxToneMapVS", L"Shaders\\SkyboxToneMap.hlsl",
        device, "SkyboxToneMapVS", "vs_5_0", defines, blob.GetAddressOf()));
    // Create InputLayout
    HR(device->CreateInputLayout(VertexPosNormalTex::GetInputLayout(), ARRAYSIZE(VertexPosNormalTex::GetInputLayout()),
        blob->GetBufferPointer(), blob->GetBufferSize(), pImpl->m_pVertexPosNormalTexLayout.ReleaseAndGetAddressOf()));


    int msaaSamples = 1;
    while (msaaSamples <= 8) 
    {
        // ***************************
        // ピクセルシェーダを作成する
        //
       
        std::string msaaSamplesStr = std::to_string(msaaSamples);
        std::string shaderNames[] =
        {
            "SkyboxToneMap_" + msaaSamplesStr + "xMSAA_PS",
            "SkyboxToneMap_STANDARD_" + msaaSamplesStr + "xMSAA_PS",
            "SkyboxToneMap_ACES_" + msaaSamplesStr + "xMSAA_PS",
            "SkyboxToneMap_ACES_COARSE_" + msaaSamplesStr + "xMSAA_PS"
        };
            

        defines[0].Name = "MSAA_SAMPLES";
        defines[0].Definition = msaaSamplesStr.c_str();
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[0], L"Shaders\\SkyboxToneMap.hlsl",
            device, "SkyboxToneMapCubePS", "ps_5_0", defines));

        defines[1].Name = "TONEMAP_STANDARD";
        defines[1].Definition = "";
        
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[1], L"Shaders\\SkyboxToneMap.hlsl",
            device, "SkyboxToneMapCubePS", "ps_5_0", defines));

        defines[1].Name = "TONEMAP_ACES";
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[2], L"Shaders\\SkyboxToneMap.hlsl",
            device, "SkyboxToneMapCubePS", "ps_5_0", defines));

        defines[1].Name = "SkyboxToneMap_ACES_COARSE";
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(shaderNames[3], L"Shaders\\SkyboxToneMap.hlsl",
            device, "SkyboxToneMapCubePS", "ps_5_0", defines));


        // ******************
        // パスを作成する
        //
        std::string passNames[] =
        {
            "SkyboxToneMap_" + msaaSamplesStr + "xMSAA",
            "SkyboxToneMap_STANDARD_" + msaaSamplesStr + "xMSAA",
            "SkyboxToneMap_ACES_" + msaaSamplesStr + "xMSAA",
            "SkyboxToneMap_ACES_COARSE_" + msaaSamplesStr + "xMSAA"
        };
        EffectPassDesc passDesc;
        passDesc.nameVS = "SkyboxToneMapVS";
        passDesc.namePS = shaderNames[0].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[0], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[0]);
            pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
        }

        passDesc.namePS = shaderNames[1].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[1], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[1]);
            pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
        }
        passDesc.namePS = shaderNames[2].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[2], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[2]);
            pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
        }

        passDesc.namePS = shaderNames[3].c_str();
        HR(pImpl->m_pEffectHelper->AddEffectPass(passNames[3], device, &passDesc));
        {
            auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passNames[3]);
            pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
        }

        msaaSamples <<= 1;
    }
    
    // ******************
    // サンプラーを作成する
    //
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamAnistropicWrap16x", RenderStates::SSAnistropicWrap16x.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamPointClamp", RenderStates::SSPointClamp.Get());

    // Debug
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    pImpl->m_pEffectHelper->SetDebugObjectName("SkyboxToneMapEffect");
#endif

    return true;
}

void SkyboxToneMapEffect::SetRenderDefault()
{
    std::string passName = "SkyboxToneMap_" + std::to_string(pImpl->m_MsaaLevels) + "xMSAA";
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void SkyboxToneMapEffect::SetRenderStandard()
{
    std::string passName = "SkyboxToneMap_STANDARD_" + std::to_string(pImpl->m_MsaaLevels) + "xMSAA";
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void SkyboxToneMapEffect::SetRenderACES()
{
    std::string passName = "SkyboxToneMap_ACES_" + std::to_string(pImpl->m_MsaaLevels) + "xMSAA";
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void SkyboxToneMapEffect::SetRenderACES_COARSE()
{
    std::string passName = "SkyboxToneMap_ACES_COARSE_" + std::to_string(pImpl->m_MsaaLevels) + "xMSAA";
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void XM_CALLCONV SkyboxToneMapEffect::SetWorldMatrix(DirectX::FXMMATRIX W)
{
    XMStoreFloat4x4(&pImpl->m_World, W);
}

void XM_CALLCONV SkyboxToneMapEffect::SetViewMatrix(DirectX::FXMMATRIX V)
{
    XMStoreFloat4x4(&pImpl->m_View, V);
}

void XM_CALLCONV SkyboxToneMapEffect::SetProjMatrix(DirectX::FXMMATRIX P)
{
    XMStoreFloat4x4(&pImpl->m_Proj, P);
}

void SkyboxToneMapEffect::SetMaterial(const Material& material)
{
    TextureManager& tm = TextureManager::Get();

    auto pStr = material.TryGet<std::string>("$Skybox");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_SkyboxTexture", pStr ? tm.GetTexture(*pStr) : nullptr);
}

MeshDataInput SkyboxToneMapEffect::GetInputData(const MeshData& meshData)
{
    MeshDataInput input;
    input.pInputLayout = pImpl->m_pCurrInputLayout.Get();
    input.topology = pImpl->m_Topology;
    input.pVertexBuffers = {
        meshData.m_pVertices.Get(),
        meshData.m_pNormals.Get(),
        meshData.m_pTexcoordArrays.empty() ? nullptr : meshData.m_pTexcoordArrays[0].Get()
    };
    input.strides = { 12, 12, 8 };
    input.offsets = { 0, 0, 0 };

    input.pIndexBuffer = meshData.m_pIndices.Get();
    input.indexCount = meshData.m_IndexCount;

    return input;
}

void SkyboxToneMapEffect::SetDepthTexture(ID3D11ShaderResourceView* depthTexture)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_DepthTexture", depthTexture);
}

void SkyboxToneMapEffect::SetLitTexture(ID3D11ShaderResourceView* litTexture)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_LitTexture", litTexture);
}

void SkyboxToneMapEffect::SetMsaaSamples(UINT msaaSamples)
{
    pImpl->m_MsaaLevels = msaaSamples;
}

void SkyboxToneMapEffect::SetSSAOTexture(ID3D11ShaderResourceView* ssaoTexture)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_SSAOTexture", ssaoTexture);
}

void SkyboxToneMapEffect::SetFlatLitTexture(ID3D11ShaderResourceView* flatLitTexture, UINT width, UINT height)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_FlatLitTexture", flatLitTexture);
    UINT wh[2] = { width, height };
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_FramebufferDimensions")->SetUIntVector(2, wh);
}

void SkyboxToneMapEffect::SetSkybox(ID3D11ShaderResourceView* skybox)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_SkyboxTexture", skybox);
}

void SkyboxToneMapEffect::Apply(ID3D11DeviceContext* deviceContext)
{
    XMMATRIX W = XMLoadFloat4x4(&pImpl->m_World);
    XMMATRIX V = XMLoadFloat4x4(&pImpl->m_View);
    XMMATRIX P = XMLoadFloat4x4(&pImpl->m_Proj);

    XMMATRIX VP = XMLoadFloat4x4(&pImpl->m_View) * XMLoadFloat4x4(&pImpl->m_Proj);
    VP = XMMatrixTranspose(VP);

    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_ViewProj")->SetFloatMatrix(4, 4, (const FLOAT*)&VP);
    pImpl->m_pCurrEffectPass->Apply(deviceContext);
}