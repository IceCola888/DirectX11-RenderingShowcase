#include "Effects.h"
#include <XUtil.h>
#include <RenderStates.h>
#include <EffectHelper.h>
#include <DXTrace.h>
#include <Vertex.h>
#include <TextureManager.h>

using namespace DirectX;

#pragma warning(disable: 26812)

static void GenerateGaussianWeights(float weights[16], int kernelSize, float sigma)
{
    float twoSigmaSq = 2.0f * sigma * sigma;
    int radius = kernelSize / 2;
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i)
    {
        float x = (float)i;

        weights[radius + i] = expf(-x * x / twoSigmaSq);

        sum += weights[radius + i];
    }

    // 重みを正規化して、合計が1.0になるようにします
    for (int i = 0; i <= kernelSize; ++i)
    {
        weights[i] /= sum;
    }
}

//
// ShadowEffect::Impl は ShadowEffect の定義より前に
//

class ShadowEffect::Impl
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
    D3D11_PRIMITIVE_TOPOLOGY m_Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

    ComPtr<ID3D11InputLayout> m_pVertexPosNormalTexLayout;

    XMFLOAT4X4 m_World{}, m_View{}, m_Proj{};

    // ブラーのカーネルサイズ（奇数である必要あり）
    int     m_BlurSize = 5;
    // ガウスブラー用の重み配列
    float   m_Weights[16]{};
    // ブラーの強さを制御するシグマ値
    float   m_BlurSigma = 1.0f;
};


//
// ShadowEffect
//

namespace
{
    static ShadowEffect* g_pInstance = nullptr;
}

ShadowEffect::ShadowEffect()
{
    if (g_pInstance)
        throw std::exception("ShadowEffect is a singleton!");
    g_pInstance = this;
    pImpl = std::make_unique<ShadowEffect::Impl>();
}

ShadowEffect::~ShadowEffect()
{
}

ShadowEffect::ShadowEffect(ShadowEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
}

ShadowEffect& ShadowEffect::operator=(ShadowEffect&& moveFrom) noexcept
{
    pImpl.swap(moveFrom.pImpl);
    return *this;
}

ShadowEffect& ShadowEffect::Get()
{
    if (!g_pInstance)
        throw std::exception("ShadowEffect needs an instance!");
    return *g_pInstance;
}


bool ShadowEffect::InitAll(ID3D11Device* device)
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
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("FullScreenTriangleTexcoordVS", L"Shaders\\Shadow.hlsl",
        device, "FullScreenTriangleTexcoordVS", "vs_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("ShadowVS", L"Shaders\\Shadow.hlsl",
        device, "ShadowVS", "vs_5_0", nullptr, blob.GetAddressOf()));
    // Create InputLayout
    HR(device->CreateInputLayout(VertexPosNormalTex::GetInputLayout(), ARRAYSIZE(VertexPosNormalTex::GetInputLayout()),
        blob->GetBufferPointer(), blob->GetBufferSize(), pImpl->m_pVertexPosNormalTexLayout.GetAddressOf()));

    // ***************************
    // ピクセルシェーダを作成する
    //

    const char* msaa_strs[] = { "1", "2", "4", "8" };
    D3D_SHADER_MACRO defines[] = {
        "MSAA_SAMPLES", "1",
        nullptr, nullptr
    };

    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("ShadowPS", L"Shaders\\Shadow.hlsl",
        device, "ShadowPS", "ps_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("DebugPS", L"Shaders\\Shadow.hlsl",
        device, "DebugPS", "ps_5_0"));

    // ESM
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("ExponentialShadowPS", L"Shaders\\Shadow.hlsl",
        device, "ExponentialShadowPS", "ps_5_0"));
    // EVSM
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("EVSM2CompPS", L"Shaders\\Shadow.hlsl",
        device, "EVSM2CompPS", "ps_5_0"));
    HR(pImpl->m_pEffectHelper->CreateShaderFromFile("EVSM4CompPS", L"Shaders\\Shadow.hlsl",
        device, "EVSM4CompPS", "ps_5_0"));

    // ******************
    // パスを作成する
    //
    EffectPassDesc passDesc;
    passDesc.nameVS = "ShadowVS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("DepthOnly", device, &passDesc));
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass("DepthOnly");
    pPass->SetRasterizerState(RenderStates::RSShadow.Get());


    passDesc.namePS = "ShadowPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("ShadowAlphaClip", device, &passDesc));
    pPass = pImpl->m_pEffectHelper->GetEffectPass("ShadowAlphaClip");
    pPass->SetRasterizerState(RenderStates::RSShadow.Get());

    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    passDesc.namePS = "DebugPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("Debug", device, &passDesc));

    std::string psName = "VarianceShadowPS_1xMSAA";
    std::string passName = "VarianceShadow_1xMSAA";
    for (const char* str : msaa_strs)
    {
        defines[0].Definition = str;
        passName[15] = *str;
        psName[17] = *str;

        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(psName, L"Shaders\\Shadow.hlsl",
            device, "VarianceShadowPS", "ps_5_0", defines));

        passDesc.nameVS = "FullScreenTriangleTexcoordVS";
        passDesc.namePS = psName;
        HR(pImpl->m_pEffectHelper->AddEffectPass(passName, device, &passDesc));
    }

    // ESM
    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    passDesc.namePS = "ExponentialShadowPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("ExponentialShadow", device, &passDesc));

    // EVSM2
    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    passDesc.namePS = "EVSM2CompPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("EVSM2Comp", device, &passDesc));
    // EVSM4
    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    passDesc.namePS = "EVSM4CompPS";
    HR(pImpl->m_pEffectHelper->AddEffectPass("EVSM4Comp", device, &passDesc));

    // Blur
    const char* kernel_strs[] = {
        "3", "5", "7", "9", "11", "13", "15",
    };
    passDesc.nameVS = "FullScreenTriangleTexcoordVS";
    defines[0].Name = "BLUR_KERNEL_SIZE";
    for (const char* str : kernel_strs)
    {
        // Blur X
        defines[0].Definition = str;

        psName = "GaussianBlurXPS_";
        psName += str;
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(psName, L"Shaders\\Shadow.hlsl",
            device, "GaussianBlurXPS", "ps_5_0", defines));
        passDesc.namePS = psName;
        passName = "GaussianBlurXPS_";
        passName += str;
        HR(pImpl->m_pEffectHelper->AddEffectPass(passName, device, &passDesc));

        // Blur Y
        psName = "GaussianBlurYPS_";
        psName += str;
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(psName, L"Shaders\\Shadow.hlsl",
            device, "GaussianBlurYPS", "ps_5_0", defines));
        passDesc.namePS = psName;
        passName = "GaussianBlurYPS_";
        passName += str;
        HR(pImpl->m_pEffectHelper->AddEffectPass(passName, device, &passDesc));

        // ESM/EVSM Blur
        psName = "LogGaussianBlurPS_";
        psName += str;
        HR(pImpl->m_pEffectHelper->CreateShaderFromFile(psName, L"Shaders\\Shadow.hlsl",
            device, "LogGaussianBlurPS", "ps_5_0", defines));
        passDesc.namePS = psName;
        passName = "LogGaussianBlur_";
        passName += str;
        HR(pImpl->m_pEffectHelper->AddEffectPass(passName, device, &passDesc));
    }

    // ******************
    // サンプラーを作成する
    //
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamLinearWrap", RenderStates::SSLinearWrap.Get());
    pImpl->m_pEffectHelper->SetSamplerStateByName("g_SamplerPointClamp", RenderStates::SSPointClamp.Get());

    // Debug
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    SetDebugObjectName(pImpl->m_pVertexPosNormalTexLayout.Get(), "ShadowEffect.VertexPosNormalTexLayout");
    pImpl->m_pEffectHelper->SetDebugObjectName("ShadowEffect");
#endif

    return true;
}



void XM_CALLCONV ShadowEffect::SetWorldMatrix(DirectX::FXMMATRIX W)
{
    XMStoreFloat4x4(&pImpl->m_World, W);
}

void XM_CALLCONV ShadowEffect::SetViewMatrix(DirectX::FXMMATRIX V)
{
    XMStoreFloat4x4(&pImpl->m_View, V);
}

void XM_CALLCONV ShadowEffect::SetProjMatrix(DirectX::FXMMATRIX P)
{
    XMStoreFloat4x4(&pImpl->m_Proj, P);
}

void ShadowEffect::SetMaterial(const Material& material)
{
    TextureManager& tm = TextureManager::Get();

    auto pStr = material.TryGet<std::string>("$Albedo");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_AlbedoMap", pStr ? tm.GetTexture(*pStr) : nullptr);
}

MeshDataInput ShadowEffect::GetInputData(const MeshData& meshData)
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

void ShadowEffect::Apply(ID3D11DeviceContext* deviceContext)
{
    XMMATRIX WVP = XMLoadFloat4x4(&pImpl->m_World) * XMLoadFloat4x4(&pImpl->m_View) * XMLoadFloat4x4(&pImpl->m_Proj);
    WVP = XMMatrixTranspose(WVP);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_WorldViewProj")->SetFloatMatrix(4, 4, (const FLOAT*)&WVP);

    pImpl->m_pCurrEffectPass->Apply(deviceContext);
}

void ShadowEffect::SetRenderDepthOnly()
{
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("DepthOnly");
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

}

void ShadowEffect::SetRenderAlphaClip(float alphaClipValue)
{
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("ShadowAlphaClip");
    pImpl->m_pCurrEffectPass->PSGetParamByName("clipValue")->SetFloat(alphaClipValue);
    pImpl->m_pCurrInputLayout = pImpl->m_pVertexPosNormalTexLayout.Get();
    pImpl->m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void ShadowEffect::RenderVarianceShadow(ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp)
{
    // フルスクリーントライアングルの設定
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTex;
    D3D11_TEXTURE2D_DESC texDesc;
    input->GetResource(reinterpret_cast<ID3D11Resource**>(pTex.GetAddressOf()));
    pTex->GetDesc(&texDesc);

    std::string passName = "VarianceShadow_1xMSAA";
    passName[15] = '0' + texDesc.SampleDesc.Count;

    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pCurrEffectPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::RenderExponentialShadow(ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp, float magic_power)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("ExponentialShadow");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pCurrEffectPass->PSGetParamByName("c")->SetFloat(magic_power);
    pImpl->m_pCurrEffectPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::RenderExponentialVarianceShadow(
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp,
    float posExp,
    float* optNegExp)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    float exps[2] = { posExp };
    if (optNegExp)
    {
        pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("EVSM4Comp");
        exps[1] = *optNegExp;
    }
    else
        pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("EVSM2Comp");

    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_EvsmExponents")->SetFloatVector(2, exps);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pCurrEffectPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::RenderDepthToTexture(ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pImpl->m_pCurrEffectPass = pImpl->m_pEffectHelper->GetEffectPass("Debug");
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pCurrEffectPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::GaussianBlurX(
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    std::string passName = "GaussianBlurXPS_" + std::to_string(pImpl->m_BlurSize);
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_BlurWeightsArray")->SetRaw(pImpl->m_Weights);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::GaussianBlurY(
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    std::string passName = "GaussianBlurYPS_" + std::to_string(pImpl->m_BlurSize);
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_BlurWeightsArray")->SetRaw(pImpl->m_Weights);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::LogGaussianBlur(ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* input, ID3D11RenderTargetView* output,
    const D3D11_VIEWPORT& vp)
{
    // フルスクリーントライアングルの設定
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    std::string passName = "LogGaussianBlur_" + std::to_string(pImpl->m_BlurSize);
    auto pPass = pImpl->m_pEffectHelper->GetEffectPass(passName);
    pImpl->m_pEffectHelper->SetShaderResourceByName("g_TextureShadow", input);
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_BlurWeightsArray")->SetRaw(pImpl->m_Weights);
    pPass->Apply(deviceContext);
    deviceContext->OMSetRenderTargets(1, &output, nullptr);
    deviceContext->RSSetViewports(1, &vp);
    deviceContext->Draw(3, 0);

    // -----------------------------
    // 使用済みリソースのスロット解放
    // -----------------------------
    int slot = pImpl->m_pEffectHelper->MapShaderResourceSlot("g_TextureShadow");
    input = nullptr;
    deviceContext->PSSetShaderResources(slot, 1, &input);
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void ShadowEffect::SetBlurKernelSize(int size)
{
    if (size % 2 == 0 || size > 15)
        return;

    pImpl->m_BlurSize = size;
    GenerateGaussianWeights(pImpl->m_Weights, pImpl->m_BlurSize, pImpl->m_BlurSigma);
}

void ShadowEffect::SetBlurSigma(float sigma)
{
    if (sigma < 0.0f)
        return;

    pImpl->m_BlurSigma = sigma;
    GenerateGaussianWeights(pImpl->m_Weights, pImpl->m_BlurSize, pImpl->m_BlurSigma);
}

void ShadowEffect::SetShadowSize(int size)
{
    pImpl->m_pEffectHelper->GetConstantBufferVariable("g_TexelSize")->SetFloat(1.0f / float(size));
}