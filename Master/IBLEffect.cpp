#include "Effects.h"
#include <EffectHelper.h>
#include <RenderStates.h>
#include <XUtil.h>
#include <DXTrace.h>
#include <Vertex.h>
#include <TextureManager.h>
using namespace DirectX;

class IBLEffect::Impl
{
public:
    
    Impl() {
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
    D3D11_PRIMITIVE_TOPOLOGY m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    ComPtr<ID3D11InputLayout> m_pVertexPosLayout;

    XMFLOAT4X4 m_View, m_Proj;
};

namespace
{
    // IBLEffectシングルトン
    static IBLEffect* g_pInstance = nullptr;
}

IBLEffect::IBLEffect()
{
    if (g_pInstance)
        throw std::exception("IBLEffect is a singleton!");
    g_pInstance = this;
    pImpl = std::make_unique<IBLEffect::Impl>();
}

IBLEffect::~IBLEffect()
{
}

IBLEffect::IBLEffect(IBLEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
}

IBLEffect& IBLEffect::operator=(IBLEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
    return *this;
}

IBLEffect& IBLEffect::Get()
{
    if (!g_pInstance)
        throw std::exception("IBLEffect needs an instance!");
    return *g_pInstance;
}

bool IBLEffect::InitAll(ID3D11Device* device)
{
    if (!device)
        return false;

    if (!RenderStates::IsInit())
        throw std::exception("RenderStates need to be initialized first!");

    pImpl->m_pEffectHelper = std::make_unique<EffectHelper>();

    pImpl->m_pEffectHelper->SetBinaryCacheDirectory(L"Shaders\\Cache");
    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    // ***********************
    // 頂点シェーダを作成する
    //
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("FullScreenTriangleTexcoordVS", L"Shaders\\BRDF_LUT.hlsl",
        device, "FullScreenTriangleTexcoordVS", "vs_5_0", nullptr, blob.ReleaseAndGetAddressOf()));

    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("EnvCubeMapVS", L"Shaders\\HDRtoCubeMap.hlsl",
        device, "EnvCubeMapVS", "vs_5_0", nullptr, blob.ReleaseAndGetAddressOf()));
    // 頂点レイアウトを作成する
    HR(device->CreateInputLayout(VertexPos::GetInputLayout(), ARRAYSIZE(VertexPos::GetInputLayout()),
        blob->GetBufferPointer(), blob->GetBufferSize(), pImpl->m_pVertexPosLayout.ReleaseAndGetAddressOf()));

    // ***************************
    // ピクセルシェーダを作成する
    //
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("HDRtoCubeMapPS", L"Shaders\\HDRtoCubeMap.hlsl", device,
        "HDRtoCubeMapPS", "ps_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("IrradianceCubeMapPS", L"Shaders\\IrradianceMap.hlsl", device,
        "IrradianceCubeMapPS", "ps_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("PrefilterCubeMapPS", L"Shaders\\PrefilterEnv.hlsl", device,
        "PrefilterCubeMapPS", "ps_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("BRDF_LUT_PS", L"Shaders\\BRDF_LUT.hlsl", device,
        "BRDF_LUT_PS", "ps_5_0"));

    // ******************
    // パスを作成する
    //
    EffectPassDesc passDesc;

    // HDR -> CubeMap
    passDesc.nameVS = "EnvCubeMapVS";
    passDesc.namePS = "HDRtoCubeMapPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("HDRtoCubeMap", device, &passDesc));
    {
        auto pPass = pImpl->m_pEffectHelper->GetEffectPass("HDRtoCubeMap");
        pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
    }

    // CubeMap -> IrradianceMap
    passDesc.nameVS = "EnvCubeMapVS";
    passDesc.namePS = "IrradianceCubeMapPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("IrradianceCubeMap", device, &passDesc));
    {
        auto pPass = pImpl->m_pEffectHelper->GetEffectPass("IrradianceCubeMap");
        pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
    }

    // CubeMap -> PrefilterEnv
    passDesc.nameVS = "EnvCubeMapVS";
    passDesc.namePS = "PrefilterCubeMapPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("PrefilterCubeMap", device, &passDesc));
    {
        auto pPass = pImpl->m_pEffectHelper->GetEffectPass("PrefilterCubeMap");
        pPass->SetRasterizerState(RenderStates::RSNoCull.Get());
    }

    // BRDF LUT
    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    passDesc.namePS = "BRDF_LUT_PS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("BRDF_LUT", device, &passDesc));


    // ******************
    // サンプラーを作成する
    //
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamLinearClamp", RenderStates::SSLinearClamp.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamAnisotropicWarp", RenderStates::SSAnistropicWrap16x.Get());


    // デバッグオブジェクトを設定する
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    SetDebugObjectName(pImpl->m_pVertexPosLayout.Get(), "IBLEffect.m_pVertexPosLayout");
#endif
    pImpl->m_pEffectHelper->SetDebugObjectName("IBLEffect");

    return true;
}

void XM_CALLCONV IBLEffect::SetWorldMatrix(DirectX::FXMMATRIX W)
{
    UNREFERENCED_PARAMETER(W);
}

void XM_CALLCONV IBLEffect::SetViewMatrix(DirectX::FXMMATRIX V)
{
    XMStoreFloat4x4(&pImpl->m_View, V);
}

void XM_CALLCONV IBLEffect::SetProjMatrix(DirectX::FXMMATRIX P)
{
    XMStoreFloat4x4(&pImpl->m_Proj, P);
}

void IBLEffect::SetMaterial(const Material& material)
{
    TextureManager& tm = TextureManager::Get();

    const std::string& str = material.Get<std::string>("$EnvironmentMap_HDR");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_EquirectangularMap", tm.GetTexture(str));
}

MeshDataInput IBLEffect::GetInputData(const MeshData& meshData)
{
    MeshDataInput input;
    input.pInputLayout = pImpl->m_pCurrInputLayout.Get();
    input.topology = pImpl->m_CurrTopology;
    input.pVertexBuffers = {
        meshData.m_pVertices.Get()
    };
    input.strides = { 12 };
    input.offsets = { 0 };

    input.pIndexBuffer = meshData.m_pIndices.Get();
    input.indexCount = meshData.m_IndexCount;

    return input;
}

void IBLEffect::RenderHDRtoCubeMaps()
{
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("HDRtoCubeMap");
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosLayout;
    pImpl->m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void IBLEffect::RenderIrradianceCubeMap()
{
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("IrradianceCubeMap");
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosLayout;
    pImpl->m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void IBLEffect::RenderPrefilterEnvCubeMap()
{
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("PrefilterCubeMap");
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosLayout;
    pImpl->m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void IBLEffect::RenderBRDFLUT(ID3D11DeviceContext* deviceContext, ID3D11RenderTargetView* output, const D3D11_VIEWPORT& vp)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    auto pass = pImpl->m_pEffectHelper->GetEffectPass("BRDF_LUT");
    pass->Apply(deviceContext);
    deviceContext->Draw(3, 0);

    // 解放
    output = nullptr;
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
}



void IBLEffect::SetSkyboxTexture(ID3D11ShaderResourceView* skyboxTexture)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_SkyboxTexture", skyboxTexture);
}

void IBLEffect::SetRoughness(float roughness)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_roughness")->SetFloat(roughness);
}

void IBLEffect::Apply(ID3D11DeviceContext* deviceContext)
{
    XMMATRIX V = XMLoadFloat4x4(&pImpl->m_View);
    V.r[3] = g_XMIdentityR3;
    XMMATRIX VP = V * XMLoadFloat4x4(&pImpl->m_Proj);

    VP = XMMatrixTranspose(VP);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldViewProj")->SetFloatMatrix(4, 4, (const FLOAT*)&VP);

    pImpl->m_pCurrEffectPass->Apply(deviceContext);
}

