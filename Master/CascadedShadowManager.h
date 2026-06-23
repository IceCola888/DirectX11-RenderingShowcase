#pragma once
#ifndef CASCADED_SHADOW_MANAGER_H
#define CASCADED_SHADOW_MANAGER_H

#include "WinMin.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <CameraController.h>
#include <DirectXCollision.h>
#include <memory>
#include <Texture2D.h>

enum class ShadowType
{
    ShadowType_CSM,
    ShadowType_VSM,
    ShadowType_ESM,
    ShadowType_EVSM2,
    ShadowType_EVSM4
};

enum class CascadeSelection
{
    CascadeSelection_Map,
    CascadeSelection_Interval
};

enum class CameraSelection
{
    CameraSelection_Eye,
    CameraSelection_Light,
    CameraSelection_Cascade1,
    CameraSelection_Cascade2,
    CameraSelection_Cascade3,
    CameraSelection_Cascade4,
    CameraSelection_Cascade5,
    CameraSelection_Cascade6,
    CameraSelection_Cascade7,
    CameraSelection_Cascade8,
};

enum class FitNearFar
{
    FitNearFar_ZeroOne,
    FitNearFar_CascadeAABB,
    FitNearFar_SceneAABB,
    FitNearFar_SceneAABB_Intersection
};

enum class FitProjection
{
    FitProjection_ToCascade,
    FitProjection_ToScene
};


class CascadedShadowManager {
public:
    template<class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    CascadedShadowManager() = default;
    ~CascadedShadowManager() = default;
    // コピー禁止、ムーブ操作のみ許可
    CascadedShadowManager(const CascadedShadowManager&) = delete;
    CascadedShadowManager& operator=(const CascadedShadowManager&) = delete;
    CascadedShadowManager(CascadedShadowManager&&) = default;
    CascadedShadowManager& operator=(CascadedShadowManager&&) = default;

    // シャドウ関連リソース初期化
    HRESULT InitResource(ID3D11Device* device);

    // 毎フレーム更新
    void UpdateFrame(const Camera& viewerCamera,
        const Camera& lightCamera,
        const DirectX::BoundingBox& sceneBouindingBox);

    // 各カスケードのシャドウRTV取得
    ID3D11RenderTargetView* GetCascadeRenderTargetView(size_t cascadeIndex) const { return m_pCSMTextureArray->GetRenderTarget(cascadeIndex); }

    /// シャドウマップ全体 / 個別カスケードの SRV 取得
    ID3D11ShaderResourceView* GetCascadesOutput() const { return m_pCSMTextureArray->GetShaderResource(); }
    ID3D11ShaderResourceView* GetCascadeOutput(size_t cascadeIndex) const { return m_pCSMTextureArray->GetShaderResource(cascadeIndex); }

    // シャドウ用ビューポート取得
    const D3D11_VIEWPORT& GetShadowViewport() const { return m_ShadowViewport; }
    // シャドウ行列取得
    DirectX::XMMATRIX GetShadowProjectionXM(size_t cascadeIndex) const { return XMLoadFloat4x4(&m_ShadowProj[cascadeIndex]); }

    // カスケード分割深度取得
    const float* GetCascadePartitions() const { return m_CascadePartitionsFrustum; }
    void GetCascadePartitions(float output[8]) const { memcpy_s(output, sizeof m_CascadePartitionsFrustum, m_CascadePartitionsFrustum, sizeof m_CascadePartitionsFrustum); }

    // シャドウAABB / OBB 取得
    DirectX::BoundingBox GetShadowAABB(size_t cascadeIndex) const { return m_ShadowProjBoundingBox[cascadeIndex]; }
    DirectX::BoundingOrientedBox GetShadowOBB(size_t cascadeIndex) const {
        DirectX::BoundingOrientedBox obb;
        DirectX::BoundingOrientedBox::CreateFromBoundingBox(obb, GetShadowAABB(cascadeIndex));
        return obb;
    }

    ID3D11RenderTargetView* GetTempTextureRTV() const { return m_pCSMTempTexture->GetRenderTarget(); }
    ID3D11ShaderResourceView* GetTempTextureOutput() const { return m_pCSMTempTexture->GetShaderResource(); }
    // 深度バッファアクセス
    ID3D11DepthStencilView* GetDepthBufferDSV()const { return m_pCSMDepthBuffer->GetDepthStencil(); }
    ID3D11ShaderResourceView* GetDepthBufferSRV() const { return m_pCSMDepthBuffer->GetShaderResource(); }
public:
    //
    // CSM 設定パラメータ
    //
    bool        m_FixedSizeFrustumAABB = true;          // 視錐台AABBのサイズを固定
    bool        m_MoveLightTexelSize = true;            // texel移動制限
    int         m_ShadowSize = 1024;                    // シャドウマップ解像度
    int         m_CascadeLevels = 4;                    // 利用するカスケードの数

    float		m_CascadePartitionsPercentage[8]{       // 視錐台分割パーセンテージ
        0.04f, 0.10f, 0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };

    //
    // Shadow map blur parameters
    //
    float       m_GaussianBlurSigma = 3.0f;             
    int		    m_BlurKernelSize = 5;                   

    int		    m_PCFKernelSize = 5;                    
    float       m_PCFDepthBias = 0.001f;               

    bool        m_BlendBetweenCascades = true;          
    float       m_BlendBetweenCascadesRange = 0.2f;     

    float       m_LightBleedingReduction = 0.8f;        
    float       m_MagicPower = 80.0f;                  

    float       m_PosExp = 5.0f;                        
    float       m_NegExp = 5.0f;                        

    //
    // シャドウ方式 / ビューモード / 分割方式の選択
    //
    ShadowType          m_ShadowType = ShadowType::ShadowType_CSM;
    CameraSelection     m_SelectedCamera = CameraSelection::CameraSelection_Eye;
    FitProjection       m_SelectedCascadesFit = FitProjection::FitProjection_ToCascade;
    FitNearFar          m_SelectedNearFarFit = FitNearFar::FitNearFar_SceneAABB_Intersection;
    CascadeSelection    m_SelectedCascadeSelection = CascadeSelection::CascadeSelection_Map;
private:
    // Near/Farの計算
    void XM_CALLCONV ComputeNearAndFar(float& outNearPlane, float& outFarPlane,
        DirectX::FXMVECTOR lightCameraOrthographicMinVec,
        DirectX::FXMVECTOR lightCameraOrthographicMaxVec,
        DirectX::XMVECTOR pointsInCameraView[]);

private:
    float	                        m_CascadePartitionsFrustum[8]{};    // 各カスケードのZ深度範囲
    DirectX::XMFLOAT4X4             m_ShadowProj[8]{};                  // シャドウ用正射影行列
    DirectX::BoundingBox            m_ShadowProjBoundingBox[8]{};       // 各カスケードのAABB
    D3D11_VIEWPORT                  m_ShadowViewport{};                 // シャドウ用ビューポート

    std::unique_ptr<Texture2DArray> m_pCSMTextureArray;                 // 储存各个cascade的shadowmap
    std::unique_ptr<Texture2D>      m_pCSMTempTexture;                  // 一時出力用
    std::unique_ptr<Depth2D>        m_pCSMDepthBuffer;                  // シャドウ深度バッファ

};

#endif