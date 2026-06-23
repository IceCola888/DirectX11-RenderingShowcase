#include "Effects.h"
#include <XUtil.h>
#include <RenderStates.h>
#include <EffectHelper.h>
#include <DXTrace.h>
#include <Vertex.h>
#include <TextureManager.h>
#include <ModelManager.h>

using namespace DirectX;

# pragma warning(disable: 26812)

//
// SSAOEffect::Impl は SSAOEffect の定義よりも先に必要
//

class SSAOEffect::Impl
{
public:
    // コンストラクタを明示的に指定する必要があります
    Impl() {}
    ~Impl() = default;

public:
    template<class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    std::unique_ptr<EffectHelper> m_pEffectHelper;

    std::shared_ptr<IEffectPass> m_pCurrEffectPass;
    ComPtr<ID3D11InputLayout> m_pCurrInputLayout;
    D3D11_PRIMITIVE_TOPOLOGY m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    ComPtr<ID3D11InputLayout> m_pVertexPosNormalTexLayout;

    ComPtr<ID3D11SamplerState> m_pSamNormalDepth;
    ComPtr<ID3D11SamplerState> m_pSamRandomVec;
    ComPtr<ID3D11SamplerState> m_pSamBlur;

    XMFLOAT4X4 m_World{}, m_View{}, m_Proj{};
};

//
// SSAOEffect
//

namespace
{
    static SSAOEffect* g_pInstance = nullptr;
}

SSAOEffect::SSAOEffect()
{
    if (g_pInstance)
        throw std::exception("SSAOEffect is a singleton!");
    g_pInstance = this;
    pImpl = std::make_unique<SSAOEffect::Impl>();
}

SSAOEffect::~SSAOEffect()
{
}

SSAOEffect::SSAOEffect(SSAOEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
}

SSAOEffect& SSAOEffect::operator=(SSAOEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
    return *this;
}

SSAOEffect& SSAOEffect::Get()
{
    if (!g_pInstance)
        throw std::exception("SSAOEffect needs an instance!");
    return *g_pInstance;
}

bool SSAOEffect::InitAll(ID3D11Device* device)
{
    if (!device)
        return false;

    if (!RenderStates::IsInit())
        throw std::exception("RenderStates need to be initialized first!");

    pImpl->m_pEffectHelper = std::make_unique<EffectHelper>();

    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    pImpl->m_pEffectHelper->SetBinaryCacheDirectory(L"Shaders\\Cache");

    // ******************
    // 頂点シェーダを作成する
    //
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("FullScreenTriangleTexcoordVS", L"Shaders\\SSAO.hlsl",
        device, "FullScreenTriangleTexcoordVS", "vs_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("SSAO_GeometryVS", L"Shaders\\SSAO.hlsl",
        device, "GeometryVS", "vs_5_0", nullptr, blob.ReleaseAndGetAddressOf()));
    // Create InputLayout
    HR(device->CreateInputLayout(VertexPosNormalTex::GetInputLayout(), ARRAYSIZE(VertexPosNormalTex::GetInputLayout()),
        blob->GetBufferPointer(), blob->GetBufferSize(), pImpl->m_pVertexPosNormalTexLayout.GetAddressOf()));

    // ***************************
    // ピクセルシェーダを作成する
    //

    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("DebugAO_PS", L"Shaders\\SSAO.hlsl",
        device, "DebugAO_PS", "ps_5_0"));
    
    // ******************
    // コンピュートシェーダーの作成
    //
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("SSAO_CS", L"Shaders\\SSAO.hlsl",
        device, "SSAO_CS", "cs_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("Blur_Horz_CS", L"Shaders\\SSAO.hlsl", 
        device, "Blur_Horz_CS", "cs_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("Blur_Vert_CS", L"Shaders\\SSAO.hlsl", 
        device, "Blur_Vert_CS", "cs_5_0"));

    // ******************
    // パスを作成する
    //
    EffectPassDesc passDesc;
    passDesc.nameVS = "SSAO_GeometryVS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("DepthOnly", device, &passDesc));
    {
        auto pPass = pImpl->m_pEffectHelper->GetEffectPass("DepthOnly");
        // Reversed-Z => GREATER_EQUALテスト
        pPass->SetDepthStencilState(RenderStates::DSSGreaterEqual.Get(), 0);
    }

    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    passDesc.namePS = "DebugAO_PS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("DebugAO", device, &passDesc));

    passDesc.nameVS = "";
    passDesc.namePS = "";
    passDesc.nameCS = "SSAO_CS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("SSAO_CS", device, &passDesc));

    passDesc.nameCS = "Blur_Horz_CS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("BlurHorz_CS", device, &passDesc));
    passDesc.nameCS = "Blur_Vert_CS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("BlurVert_CS", device, &passDesc));

    // ******************
    // サンプラーを作成する
    //
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamLinearWrap", RenderStates::SSLinearWrap.Get());

    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory(&samplerDesc, sizeof samplerDesc);

    // 法線と深度のサンプリングに使用するサンプラー
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HR(device->CreateSamplerState(&samplerDesc, pImpl->m_pSamNormalDepth.GetAddressOf()));
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamNormalDepth", pImpl->m_pSamNormalDepth.Get());

    // ランダムベクトルのサンプリングに使用するサンプラー
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.BorderColor[3] = 0.0f;
    HR(device->CreateSamplerState(&samplerDesc, pImpl->m_pSamRandomVec.GetAddressOf()));
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamRandomVec", pImpl->m_pSamRandomVec.Get());

    // Debug
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    SetDebugObjectName(pImpl->m_pVertexPosNormalTexLayout.Get(), "SSAOEffect.VertexPosNormalTexLayout");
    SetDebugObjectName(pImpl->m_pSamNormalDepth.Get(), "SSAOEffect.SSNormalDepth");
    SetDebugObjectName(pImpl->m_pSamRandomVec.Get(), "SSAOEffect.SSRandomVec");

#endif

    pImpl->m_pEffectHelper->SetDebugObjectName("SSAOEffect");

    
    return true;
}

void XM_CALLCONV SSAOEffect::SetWorldMatrix(DirectX::FXMMATRIX W)
{
    XMStoreFloat4x4(&pImpl->m_World, W);
}

void XM_CALLCONV SSAOEffect::SetViewMatrix(DirectX::FXMMATRIX V)
{
    XMStoreFloat4x4(&pImpl->m_View, V);
}

void XM_CALLCONV SSAOEffect::SetProjMatrix(DirectX::FXMMATRIX P)
{
    XMStoreFloat4x4(&pImpl->m_Proj, P);
}

void SSAOEffect::SetMaterial(const Material& material)
{
    TextureManager& tm = TextureManager::Get();

    auto pStr = material.TryGet<std::string>("$Diffuse");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_DiffuseMap", pStr ? tm.GetTexture(*pStr) : tm.GetNullTexture());
}

MeshDataInput SSAOEffect::GetInputData(const MeshData& meshData)
{
    MeshDataInput input;
    input.pInputLayout = pImpl->m_pCurrInputLayout.Get();
    input.topology = pImpl->m_CurrTopology;
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

void SSAOEffect::SetRenderNormalDepthMap(ID3D11ShaderResourceView* normalGBuffer, bool enableAlphaClip)
{
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("SSAO_Geometry");
    pImpl->m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pImpl->m_pCurrEffectPass->PSGetParamByName("alphaClip")->SetUInt(enableAlphaClip);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_NormalGBuffer", normalGBuffer);
}

void SSAOEffect::SetRenderDepthOnly()
{
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("DepthOnly");
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_CurrTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void SSAOEffect::SetTextureRandomVec(ID3D11ShaderResourceView* textureRandomVec)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_RandomVecMap", textureRandomVec);
}

void SSAOEffect::SetOffsetVectors(const DirectX::XMFLOAT4 offsetVectors[14])
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_OffsetVectors")->SetRaw(offsetVectors);
}

void SSAOEffect::RenderToSSAOCS(ID3D11DeviceContext* deviceContext, 
    ID3D11ShaderResourceView* normalGBuffer,
    ID3D11ShaderResourceView* depthGBuffer,
    ID3D11UnorderedAccessView* output, 
    const D3D11_VIEWPORT& vp, uint32_t sampleCount)
{
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_NormalGBuffer", normalGBuffer);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_DepthBuffer", depthGBuffer);
    pImpl->m_pEffectHelper->SetUnorderedAccessByName("g_SSAOOutput", output);
    
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass("SSAO_CS");
    pPass->CSGetParamByName("sampleCount")->SetUInt(sampleCount);
    pPass->Apply(deviceContext);
    pPass->Dispatch(deviceContext, uint32_t(vp.Width), uint32_t(vp.Height));

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    normalGBuffer = nullptr;
    depthGBuffer = nullptr;
    output = nullptr;
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_NormalGBuffer"), 1, &normalGBuffer);
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_DepthBuffer"), 1, &depthGBuffer);
    deviceContext->CSSetUnorderedAccessViews(pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_SSAOOutput"), 1, &output, nullptr);
}

void SSAOEffect::SetOcclusionInfo(float radius, float fadeStart, float fadeEnd, float surfaceEpsilon)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_OcclusionRadius")->SetFloat(radius);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_OcclusionFadeStart")->SetFloat(fadeStart);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_OcclusionFadeEnd")->SetFloat(fadeEnd);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_SurfaceEpsilon")->SetFloat(surfaceEpsilon);
}

void SSAOEffect::SetFrustumFarPlanePoints(const DirectX::XMFLOAT4 farPlanePoints[3])
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_FarPlanePoints")->SetRaw(farPlanePoints);
}

void SSAOEffect::SetFrustumFarPlaneSquardPoints(const DirectX::XMFLOAT4 farPlanePoints[4])
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_FarPlaneSquarePoints")->SetRaw(farPlanePoints);
}

void SSAOEffect::SetSSAOTextureSize(uint32_t ssaoX, uint32_t ssaoY)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_SSAOX")->SetUInt(ssaoX);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_SSAOY")->SetUInt(ssaoY);
}

void SSAOEffect::SetBlurWeights(const float weights[11])
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_BlurWeights")->SetRaw(weights);
}

void SSAOEffect::SetBlurRadius(int radius)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_BlurRadius")->SetSInt(radius);
}

void SSAOEffect::ComputeBlurX(ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* inputSSAO, 
    ID3D11ShaderResourceView* inputNor, 
    ID3D11ShaderResourceView* inputDep,
    ID3D11UnorderedAccessView* output, 
    uint32_t width, uint32_t height)
{
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass("BlurHorz_CS");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_InputImage", inputSSAO);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_NormalGBuffer", inputNor);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_DepthBuffer", inputDep);
    pImpl->m_pEffectHelper->SetUnorderedAccessByName("g_BlurOutput", output);
    pPass->Apply(deviceContext);
    pPass->Dispatch(deviceContext, width, height);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    inputSSAO = nullptr;
    inputNor = nullptr;
    inputDep = nullptr;
    output = nullptr;
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_InputImage"), 1, &inputSSAO);
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_NormalGBuffer"), 1, &inputNor);
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_DepthBuffer"), 1, &inputDep);
    deviceContext->CSSetUnorderedAccessViews(pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_BlurOutput"), 1, &output, nullptr);
}

void SSAOEffect::ComputeBlurY(ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* inputSSAO, 
    ID3D11ShaderResourceView* inputNor, 
    ID3D11ShaderResourceView* inputDep,
    ID3D11UnorderedAccessView* output, 
    uint32_t width, uint32_t height)
{
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass("BlurVert_CS");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_InputImage", inputSSAO);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_NormalGBuffer", inputNor);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_DepthBuffer", inputDep);
    pImpl->m_pEffectHelper->SetUnorderedAccessByName("g_BlurOutput", output);
    pPass->Apply(deviceContext);
    pPass->Dispatch(deviceContext, width, height);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    inputSSAO = nullptr;
    inputNor = nullptr;
    inputDep = nullptr;
    output = nullptr;
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_InputImage"), 1, &inputSSAO);
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_NormalGBuffer"), 1, &inputNor);
    deviceContext->CSSetShaderResources(pImpl->m_pEffectHelper->MapShaderResourceSlot("g_DepthBuffer"), 1, &inputDep);
    deviceContext->CSSetUnorderedAccessViews(pImpl->m_pEffectHelper->MapUnorderedAccessSlot("g_BlurOutput"), 1, &output, nullptr);
}

void SSAOEffect::RenderAmbientOcclusionToTexture(
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp)
{
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("DebugAO");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_DiffuseMap", input);
    pImpl->m_pCurrEffectPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_DiffuseMap");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void SSAOEffect::Apply(ID3D11DeviceContext* deviceContext)
{
    XMMATRIX W = XMLoadFloat4x4(&pImpl->m_World);
    XMMATRIX V = XMLoadFloat4x4(&pImpl->m_View);
    XMMATRIX P = XMLoadFloat4x4(&pImpl->m_Proj);

    XMMATRIX WV = W * V;
    XMMATRIX WVP = WV * P;
    XMMATRIX WInvTV = XMath::InverseTranspose(W) * V;
    XMMATRIX InvP = XMMatrixInverse(nullptr, P);


    // NDC空間 [-1, 1]^2 からテクスチャ空間 [0, 1]^2 への変換
    static const XMMATRIX T = XMMATRIX(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);
    // ビュー空間からテクスチャ空間への変換
    XMMATRIX PT = P * T;

    PT = XMMatrixTranspose(PT);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_ViewToTexSpace")->SetFloatMatrix(4, 4, (const FLOAT*)&PT);

    WV = XMMatrixTranspose(WV);
    WInvTV = XMMatrixTranspose(WInvTV);
    WVP = XMMatrixTranspose(WVP);
    InvP = XMMatrixTranspose(InvP);
    P = XMMatrixTranspose(P);

    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldView")->SetFloatMatrix(4, 4, (const FLOAT*)&WV);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldViewProj")->SetFloatMatrix(4, 4, (const FLOAT*)&WVP);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldInvTransposeView")->SetFloatMatrix(4, 4, (const FLOAT*)&WInvTV);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_InvProj")->SetFloatMatrix(4, 4, (const FLOAT*)&InvP);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_Proj")->SetFloatMatrix(4, 4, (const FLOAT*)&P);

    if (pImpl->m_pCurrEffectPass)
        pImpl->m_pCurrEffectPass->Apply(deviceContext);
}