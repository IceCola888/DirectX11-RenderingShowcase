#pragma once
#ifndef EFFECTS_H
#define EFFECTS_H

#include <IEffect.h>
#include <Material.h>
#include <MeshData.h>
#include <LightHelper.h>
#include <GameObject.h>

class DeferredEffect : public IEffect, public IEffectTransform,
    public IEffectMaterial, public IEffectMeshData
{
public:

    DeferredEffect();
    virtual ~DeferredEffect() override;

    DeferredEffect(DeferredEffect&& moveFrom) noexcept;
    DeferredEffect& operator=(DeferredEffect&& moveFrom) noexcept;

    // シングルトンインスタンス取得
    static DeferredEffect& Get();

    // リソース初期化
    bool InitAll(ID3D11Device* device);

    //
    // IEffectTransform
    //

    void XM_CALLCONV SetWorldMatrix(DirectX::FXMMATRIX W) override;
    void XM_CALLCONV SetViewMatrix(DirectX::FXMMATRIX V) override;
    void XM_CALLCONV SetProjMatrix(DirectX::FXMMATRIX P) override;

    //
    // IEffectMaterial
    //

    void SetMaterial(const Material& material) override;

    //
    // IEffectMeshData
    //

    MeshDataInput GetInputData(const MeshData& meshData) override;

    // 
    // DeferredEffect
    //

    // MSAAサンプル数設定
    void SetMsaaSamples(UINT msaaSamples);
    // GBuffer描画
    void SetRenderGBuffer(bool alphaClip);
    // MSAA部分の可視化
    void SetVisualizeShadingFreq(bool enable);
    // ライト数可視化
    void SetVisualizeLightCount(bool enable);
    // IBL利用設定
    void SetUseIBL(bool enable);
    // シャドウタイプ設定
    void SetShadowType(int type);
    // CSMレベル設定
    void SetCascadeLevels(int cascadeLevels);
    // カスケード間のブレンド領域
    void SetCascadeBlendArea(float blendArea);
    // カスケード可視化
    void SetCascadeVisulization(bool enable);

    void SetCascadeIntervalSelectionEnabled(bool enable);

    void SetCascadeOffsets(const DirectX::XMFLOAT4 offsets[4]);
    void SetCascadeScales(const DirectX::XMFLOAT4 scales[4]);
    // フラスタムのZ範囲
    void SetCascadeFrustumsEyeSpaceDepths(const float depths[8]);

    // VSM防漏光
    void SetLightBleedingReduction(float value);
    // ESM
    void SetMagicPower(float power);
    // EVSM2
    void SetPosExponent(float posExp);
    // EVSM4
    void SetNegExponent(float negExp);
    // PCFのカーネルサイズ
    void SetPCFKernelSize(int size);
    // PCFの深度バイアス
    void SetPCFDepthBias(float bias);
    // シャドウサイズ
    void SetShadowSize(int size);
    // シャドウマップ配列設定
    void SetShadowTextureArray(ID3D11ShaderResourceView* shadow);
    // // シャドウ用ビュー行列設定
    void XM_CALLCONV SetShadowViewMatrix(DirectX::FXMMATRIX ShadowView);
    // カメラのZ範囲設定
    void SetCameraNearFar(float nearZ, float farZ);
    // View Spaceの方向光設定
    void SetDirectionalLight(
        const DirectX::XMFLOAT3& lightDir,
        const DirectX::XMFLOAT3& lightColor,
        float lightIntensity);
    // 現在のMSAA設定を取得
    UINT GetMsaaSamples();


    // TBDR
    void ComputeTiledLightCulling(ID3D11DeviceContext* deviceContext,
        ID3D11UnorderedAccessView* litFlatBufferUAV,
        ID3D11ShaderResourceView* pointLightBufferSRV,      // View Space
        ID3D11ShaderResourceView* GBuffers[4],
        ID3D11ShaderResourceView* ssaoTexture,
        ID3D11ShaderResourceView* shadowTextureArraySRV,
        ID3D11ShaderResourceView* irradianceMap,
        ID3D11ShaderResourceView* prefilterMap,
        ID3D11ShaderResourceView* BRDFLUT);

    //
    // Debug
    // 
    
    // GBufferの法線を可視化
    void DebugNormalGBuffer(ID3D11DeviceContext* deviceContext,
        ID3D11RenderTargetView* rtv,
        ID3D11ShaderResourceView* normalGBuffer,
        D3D11_VIEWPORT viewport);

    // GBufferのRoughnessを可視化
    void DebugRoughnessGBuffer(ID3D11DeviceContext* deviceContext,
        ID3D11RenderTargetView* rtv,
        ID3D11ShaderResourceView* roughnessGBuffer,
        D3D11_VIEWPORT viewport);

    // GBufferの金属度(Metallic)を可視化
    void DebugMetallicGBuffer(ID3D11DeviceContext* deviceContext,
        ID3D11RenderTargetView* rtv,
        ID3D11ShaderResourceView* metallicGBuffer,
        D3D11_VIEWPORT viewport);

    // GBufferのアルベド(Albedo)を可視化
    void DebugAlbedoGBuffer(ID3D11DeviceContext* deviceContext,
        ID3D11RenderTargetView* rtv,
        ID3D11ShaderResourceView* albedoGBuffer,
        D3D11_VIEWPORT viewport);

    // Z勾配GBufferのデバッグ表示
    void DebugPosZGradGBuffer(ID3D11DeviceContext* deviceContext,
        ID3D11RenderTargetView* rtv,
        ID3D11ShaderResourceView* posZGradGBuffer,
        D3D11_VIEWPORT viewport);

    //
    // IEffect
    //

    // 各種定数バッファをGPUに適用
    void Apply(ID3D11DeviceContext* deviceContext) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

class IBLEffect : public IEffect, public IEffectTransform,
    public IEffectMaterial, public IEffectMeshData
{
public:
    IBLEffect();
    virtual ~IBLEffect() override;

    IBLEffect(IBLEffect&& moveFrom) noexcept;
    IBLEffect& operator=(IBLEffect&& moveFrom) noexcept;

    // シングルトンインスタンス取得
    static IBLEffect& Get();

    // リソース初期化
    bool InitAll(ID3D11Device* device);

    //
    // IEffectTransform
    //

    // いらない
    void XM_CALLCONV SetWorldMatrix(DirectX::FXMMATRIX W) override;

    void XM_CALLCONV SetViewMatrix(DirectX::FXMMATRIX V) override;
    void XM_CALLCONV SetProjMatrix(DirectX::FXMMATRIX P) override;

    //
    // IEffectMaterial
    //

    void SetMaterial(const Material& material) override;

    //
    // IEffectMeshData
    //

    MeshDataInput GetInputData(const MeshData& meshData) override;

    // 
    // IBLEffect
    //

    // HDR to CubeMap
    void RenderHDRtoCubeMaps();
    // IrradianceMap 生成
    void RenderIrradianceCubeMap();
    // Prefiltered Environment Map 生成
    void RenderPrefilterEnvCubeMap();
    // BRDF LUT 生成
    void RenderBRDFLUT(ID3D11DeviceContext* deviceContext, ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);
    // Skybox用テクスチャの指定
    void SetSkyboxTexture(ID3D11ShaderResourceView* skyboxTexture);
    // roughness 設定
    void SetRoughness(float roughness);
    //
    // IEffect
    //

    // 各種定数バッファをGPUに適用
    void Apply(ID3D11DeviceContext* deviceContext) override;
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

class SkyboxToneMapEffect : public IEffect, public IEffectTransform,
    public IEffectMeshData, public IEffectMaterial
{
public:

    // Tone Mapping 手法の列挙
    enum ToneMapping
    {
        ToneMapping_Reinhard,
        ToneMapping_Standard,
        ToneMapping_ACES,
        ToneMapping_ACES_Coarse
    };

    SkyboxToneMapEffect();
    virtual ~SkyboxToneMapEffect() override;

    SkyboxToneMapEffect(SkyboxToneMapEffect&& moveFrom) noexcept;
    SkyboxToneMapEffect& operator=(SkyboxToneMapEffect&& moveFrom) noexcept;

    // シングルトン取得
    static SkyboxToneMapEffect& Get();

    // 必要な全リソースの初期化
    bool InitAll(ID3D11Device* device);

    //
    // IEffectTransform
    //

    void XM_CALLCONV SetWorldMatrix(DirectX::FXMMATRIX W) override;
    void XM_CALLCONV SetViewMatrix(DirectX::FXMMATRIX V) override;
    void XM_CALLCONV SetProjMatrix(DirectX::FXMMATRIX P) override;

    //
    // IEffectMaterial
    //

    void SetMaterial(const Material& material) override;

    //
    // IEffectMeshData
    //

    MeshDataInput GetInputData(const MeshData& meshData) override;

    // 
    // SkyboxToneMapEffect
    //

    // デフォルト描画モード設定
    void SetRenderDefault();
    void SetRenderStandard();
    void SetRenderACES();
    void SetRenderACES_COARSE();
    // Depth Buffer 設定
    void SetDepthTexture(ID3D11ShaderResourceView* depthTexture);
    // Sceneの最終ライティング結果
    void SetLitTexture(ID3D11ShaderResourceView* litTexture);
    // SSAO結果テクスチャの設定
    void SetSSAOTexture(ID3D11ShaderResourceView* ssaoTexture);
    // 環境スカイボックス
    void SetSkybox(ID3D11ShaderResourceView* skybox);
    // MSAAレベルの設定
    void SetMsaaSamples(UINT msaaSamples);
    // TBDR（Tile-Based Deferred Rendering）の結果を設定
    void SetFlatLitTexture(ID3D11ShaderResourceView* flatLitTexture, UINT width, UINT height);


    //
    // IEffect
    //

    // 各種定数バッファをGPUに適用
    void Apply(ID3D11DeviceContext* deviceContext) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

class ShadowEffect : public IEffect, public IEffectTransform,
    public IEffectMaterial, public IEffectMeshData
{
public:
    ShadowEffect();
    virtual ~ShadowEffect() override;

    ShadowEffect(ShadowEffect&& moveFrom) noexcept;
    ShadowEffect& operator=(ShadowEffect&& moveFrom) noexcept;

    // シングルトン取得
    static ShadowEffect& Get();

    // GPUリソースの初期化
    bool InitAll(ID3D11Device* device);

    //
    // IEffectTransform
    //

    void XM_CALLCONV SetWorldMatrix(DirectX::FXMMATRIX W) override;
    void XM_CALLCONV SetViewMatrix(DirectX::FXMMATRIX V) override;
    void XM_CALLCONV SetProjMatrix(DirectX::FXMMATRIX P) override;

    //
    // IEffectMaterial
    //

    void SetMaterial(const Material& material) override;

    //
    // IEffectMeshData
    //

    MeshDataInput GetInputData(const MeshData& meshData) override;

    //
    // ShadowEffect
    //

    // 深度のみ描画
    void SetRenderDepthOnly();

    // アルファクリップ付きの描画
    void SetRenderAlphaClip(float alphaClipValue);

    // VSM のレンダリング
    void RenderVarianceShadow(ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);

    // ESM のレンダリング
    void RenderExponentialShadow(ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp,
        float magic_power);
    
    // EVSM のレンダリング
    void RenderExponentialVarianceShadow(ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp,
        float posExp, float* optNegExp = nullptr);

    // 深度マップをテクスチャへ描画
    void RenderDepthToTexture(ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);

    // input と outputのサイズを一致する必要がある
    void GaussianBlurX(
        ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);

    // input と outputのサイズを一致する必要がある
    void GaussianBlurY(
        ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);

    // input と outputのサイズを一致する必要がある
    void LogGaussianBlur(
        ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);

    // ブラーカーネルのサイズ設定（奇数: 3〜15）
    void SetBlurKernelSize(int size);
    void SetBlurSigma(float sigma);
    void SetShadowSize(int size);

    // 各種定数バッファをGPUに適用
    void Apply(ID3D11DeviceContext* deviceContext) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

};

class SSAOEffect :public IEffect, public IEffectTransform,
    public IEffectMaterial, public IEffectMeshData
{
public:

    SSAOEffect();
    virtual ~SSAOEffect() override;

    SSAOEffect(SSAOEffect&& moveFrom) noexcept;
    SSAOEffect& operator=(SSAOEffect&& moveFrom) noexcept;

    // シングルトンインスタンス取得
    static SSAOEffect& Get();

    // SSAO用リソースの初期化
    bool InitAll(ID3D11Device* device);

    //
    // IEffectTransform
    //

    void XM_CALLCONV SetWorldMatrix(DirectX::FXMMATRIX W) override;
    void XM_CALLCONV SetViewMatrix(DirectX::FXMMATRIX V) override;
    void XM_CALLCONV SetProjMatrix(DirectX::FXMMATRIX P) override;

    //
    // IEffectMaterial
    //

    void SetMaterial(const Material& material) override;

    //
    // IEffectMeshData
    //

    MeshDataInput GetInputData(const MeshData& meshData) override;

    // 
    // SSAOEffect
    //

    //
    // Pass 1: 法線・深度マップの作成
    //
    void SetRenderNormalDepthMap(ID3D11ShaderResourceView* normalGBuffer, bool enableAlphaClip = false);
    void SetRenderDepthOnly();
    //
    // Pass2: 绘制SSAO图
    // 
    
    // ランダムベクトルテクスチャの設定
    void SetTextureRandomVec(ID3D11ShaderResourceView* textureRandomVec);
    // 遮蔽パラメータの設定
    void SetOcclusionInfo(float radius, float fadeStart, float fadeEnd, float surfaceEpsilon);
    // 遠平面設定
    void SetFrustumFarPlanePoints(const DirectX::XMFLOAT4 farPlanePoints[3]);
    void SetFrustumFarPlaneSquardPoints(const DirectX::XMFLOAT4 farPlanePoints[4]);
    // SSAOテクスチャサイズの設定
    void SetSSAOTextureSize(uint32_t ssaoX, uint32_t ssaoY);
    // サンプリング用オフセットベクトルの設定
    void SetOffsetVectors(const DirectX::XMFLOAT4 offsetVectors[14]);

    // SSAOのコンピュートシェーダー
    void RenderToSSAOCS(
        ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* normalGBuffer,
        ID3D11ShaderResourceView* depthGBuffer,
        ID3D11UnorderedAccessView* output,
        const D3D11_VIEWPORT& vp,
        uint32_t sampleCount);

    //
    // Pass3: SSAO 結果に双方向フィルタを適用
    //

    // Set blur weights
    void SetBlurWeights(const float weights[11]);
    // Set blur radius
    void SetBlurRadius(int radius);
    // 横方向のブラー適用
    void ComputeBlurX(ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* inputSSAO,
        ID3D11ShaderResourceView* inputNor,
        ID3D11ShaderResourceView* inputDep,
        ID3D11UnorderedAccessView* output,
        uint32_t width, uint32_t height);
    // 縦方向のブラー適用
    void ComputeBlurY(ID3D11DeviceContext* deviceContext, ID3D11ShaderResourceView* inputSSAO,
        ID3D11ShaderResourceView* inputNor,
        ID3D11ShaderResourceView* inputDep,
        ID3D11UnorderedAccessView* output,
        uint32_t width, uint32_t height);

    // SSAO結果をテクスチャに描画
    void RenderAmbientOcclusionToTexture(ID3D11DeviceContext* deviceContext,
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        const D3D11_VIEWPORT& vp);

    //
    // IEffect
    //

    // 各種定数バッファをGPUに適用
    void Apply(ID3D11DeviceContext* deviceContext) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
#endif