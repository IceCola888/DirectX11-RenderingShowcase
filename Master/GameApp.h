#pragma once
#ifndef GAMEAPP_H
#define GAMEAPP_H

#include <random>
#include <WinMin.h>
#include <D3DApp.h>
#include <Camera.h>
#include <RenderStates.h>
#include <GameObject.h>
#include <Texture2D.h>
#include <Buffer.h>
#include <ModelManager.h>
#include <TextureManager.h>
#include <LightHelper.h>
#include "Effects.h"
#include "IBLManager.h"
#include "ShaderDefines.h"
#include "CascadedShadowManager.h"
#include "SSAOManager.h"

// PointLightアニメーション初期化データ
struct PointLightInitData
{
    float radius;
    float angle;
    float height;
    float animationSpeed;
};

// Per-tile light list used in Tile-Based Lighting
struct TileInfo
{
    UINT numLights;
    UINT lightIndices[MAX_LIGHT_INDICES];
};


class GameApp :public D3DApp
{
public:
	GameApp(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight);
	~GameApp();

    bool Init();                // リソースの初期化
    void OnResize();            // ウィンドウサイズ変更時の処理
    void UpdateScene(float dt); // 毎フレームの更新処理
    void DrawScene();           // フレームの描画処理

private:

    void DrawImGui();                                               // ImGuiデバッグUIの描画
    DirectX::XMFLOAT3 HueToRGB(float hue);                          // Hue値をRGBに変換

    void ResizeLights(UINT activeLights);                           // PointLight Buffer Resize
    void ResizeBuffers(UINT width, UINT height, UINT msaaSamples);  // Buffer Resize

    bool InitResource();                // リソースの初期化とプレフィルタリング処理
    void InitLightParams();             // ライトパラメータの初期化

    void UpdateLights(float dt);        // ライトのアニメーション更新
    void UpdateImGui(float dt);         // ImGuiUIの更新

    void RenderShadowForAllCascades();  // CSMの描画
    void RenderGBuffer();               // GBufferの描画
    void RenderSSAO();                  // SSAOの描画
    void RenderSkybox();                // skyboxの描画
   
private:
    //
    // GPUパフォーマンス測定用タイマー
    //

    GpuTimer m_GpuTimer_Shadow;
    GpuTimer m_GpuTimer_LightCulling;
    GpuTimer m_GpuTimer_Geometry;
    GpuTimer m_GpuTimer_Skybox;
    GpuTimer m_GpuTimer_SSAO;

    //
    // カメラ
    //
    std::shared_ptr<FirstPersonCamera> m_pCamera;               // メインカメラ
    std::shared_ptr<FirstPersonCamera> m_pLightCamera;          // シャドウ用ライトカメラ
    FirstPersonCameraController        m_CameraController;      // カメラコントローラー

    //
    // 各種管理クラス
    //
    CascadedShadowManager   m_CSManager;  
    TextureManager          m_TextureManager;
    ModelManager            m_ModelManager;
    SSAOManager             m_SSAOManager;
    IBLManager              m_IBLManager;

    //
    // 描画エフェクト
    //
    SkyboxToneMapEffect     m_SkyboxEffect;
    DeferredEffect          m_DeferredEffect;
    ShadowEffect            m_ShadowEffect;
    SSAOEffect              m_SSAOEffect;
    IBLEffect               m_IBLEffect;
    
    //
    // Deferred Rendering
    //
    std::unique_ptr<StructuredBuffer<DirectX::XMUINT2>>     m_pFlatLitBuffer;           // シーンレンダリングバッファ(TBDR + MSAA)
    std::vector<std::unique_ptr<Texture2DMS>>               m_pGBuffers;                // GBuffers
    std::vector<ID3D11ShaderResourceView*>                  m_pGBufferSRVs;             // GBuffersのSRVリスト
    std::vector<ID3D11RenderTargetView*>                    m_pGBufferRTVs;             // GBuffersのRTVリスト
    ComPtr<ID3D11DepthStencilView>                          m_pDepthBufferReadOnlyDSV;  // 深度読み取り専用ビュー
    std::unique_ptr<Texture2DMS>                            m_pLitBuffer;               // 照明結果出力バッファ
    std::unique_ptr<Depth2DMS>                              m_pDepthBuffer;             // 深度バッファ
    UINT                                                    m_MsaaSamples = 1;          // MSAAサンプル数
    
    // 
    // シーンオブジェクト
    //
    GameObject              m_Test;
    GameObject              m_Skybox;
    GameObject              m_Sponza;
    DirectX::XMFLOAT4       m_PlaneAlbedo = { 0.8f,0.8f,0.8f,1.0f };
    float                   m_PlaneRoughness = 0.5f;                                             
    float                   m_PlaneMetallic = 0.0f;


    //
    // Lightings
    //
    
    // PointLights
    std::unique_ptr<StructuredBuffer<PointLight>>  m_pPointLightBuffer;                 // あらゆるPointLight
    std::vector<PointLightInitData>                m_PointLightInitDatas;               // PointLight アニメーション
    std::vector<DirectX::XMFLOAT3>                 m_PointLightPosWorlds;               // PointLight World Position
    std::vector<PointLight>                        m_PointLightParams;                  // PointLight パラメータ
    UINT                                           m_ActiveLights = (MAX_LIGHTS >> 3);  // 生きているPointLightの数
    // DirectionalLight
    DirectionalLight     m_DirLight{};          // 主方向ライト
    DirectX::XMFLOAT3    m_DirLightColor{};     // 光の色
    float                m_DirLightIntensity{}; // 光の強さ


    //
    // ImGui
    //
    SkyboxToneMapEffect::ToneMapping m_ToneMapping = SkyboxToneMapEffect::ToneMapping_Standard;

    std::unique_ptr<Texture2D> m_pDebugRoughnessBuffer;
    std::unique_ptr<Texture2D> m_pDebugMetallicBuffer;
    std::unique_ptr<Texture2D> m_pDebugPosZGradBuffer;
    std::unique_ptr<Texture2D> m_pDebugNormalBuffer;                        
    std::unique_ptr<Texture2D> m_pDebugAlbedoBuffer;                                   
    std::unique_ptr<Texture2D> m_pDebugShadowBuffer;				        
    std::unique_ptr<Texture2D> m_pDebugDepthHBuffer;
    std::unique_ptr<Texture2D> m_pDebugAOTexture;

    int   m_HeightScale =       7;
    float m_LightHeightScale =  0.25f;
    float m_timeScale =         0.0f;

    bool m_VisualizeShadingFreq = false;
    bool m_VisualizeLightCount =  false;
    bool m_DebugRoughness =       false;
    bool m_DebugMetallic =        false;
    bool m_DebugPosZGrad =        false;
    bool m_ClearGBuffers =        true;
    bool m_DebugNormal =          false;                                          
    bool m_DebugAlbedo =          false;
    bool m_DebugShadow =          false;
    bool m_DebugDepthV =          false;
    bool m_UseTexture =           true;
    bool m_EnableSSAO =           true;
    bool m_DebugSSAO =            false;
    bool m_UseIBL =               false;
    
    
};

#endif