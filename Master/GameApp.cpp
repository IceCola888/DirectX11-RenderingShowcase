#include "GameApp.h"
#include <XUtil.h>
#include <DXTrace.h>
#include <ScreenGrab11.h>

using namespace DirectX;

GameApp::GameApp(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight)
    : D3DApp(hInstance, windowName, initWidth, initHeight)
{
}

GameApp::~GameApp()
{
}

bool GameApp::Init()
{
    if (!D3DApp::Init())
        return false;

    m_TextureManager.Init(m_pd3dDevice.Get());
    m_ModelManager.Init(m_pd3dDevice.Get());

    m_GpuTimer_LightCulling.Init(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get());
    m_GpuTimer_Geometry.Init(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get());
    m_GpuTimer_Shadow.Init(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get());
    m_GpuTimer_Skybox.Init(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get());
    m_GpuTimer_SSAO.Init(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get());

    // 以下のエフェクトで使用するため、すべてのレンダリングステートを必ず先に初期化すること
    RenderStates::InitAll(m_pd3dDevice.Get());

    if (!m_IBLEffect.InitAll(m_pd3dDevice.Get()))
        return false;

    if (!m_DeferredEffect.InitAll(m_pd3dDevice.Get()))
        return false;

    if (!m_ShadowEffect.InitAll(m_pd3dDevice.Get()))
        return false;

    if (!m_SSAOEffect.InitAll(m_pd3dDevice.Get()))
        return false;

    if (!m_SkyboxEffect.InitAll(m_pd3dDevice.Get()))
        return false;

    if (!InitResource())
        return false;

    return true;
}

void GameApp::OnResize()
{
    D3DApp::OnResize();

    // カメラ変更の表示
    if (m_pCamera != nullptr)
    {
        m_pCamera->SetFrustum(XM_PI / 3, AspectRatio(), 0.5f, 300.0f);
        m_pCamera->SetViewPort(0.0f, 0.0f, (float)m_ClientWidth, (float)m_ClientHeight);   
        m_DeferredEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));
        m_SSAOEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));
    }

    ResizeBuffers(m_ClientWidth, m_ClientHeight, m_MsaaSamples);
    m_SSAOManager.OnResize(m_pd3dDevice.Get(), m_ClientWidth, m_ClientHeight);
}

void GameApp::UpdateScene(float dt)
{
    if (m_CSManager.m_SelectedCamera <= CameraSelection::CameraSelection_Light)
        m_CameraController.Update(dt);

    UpdateImGui(dt);

    if (m_CSManager.m_SelectedCamera == CameraSelection::CameraSelection_Eye)
    {
        m_DeferredEffect.SetViewMatrix(m_pCamera->GetViewMatrixXM());
        m_DeferredEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));
        m_SkyboxEffect.SetViewMatrix(m_pCamera->GetViewMatrixXM());
        m_SkyboxEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));
    }
    else if (m_CSManager.m_SelectedCamera == CameraSelection::CameraSelection_Light)
    {
        // Reversed - Z
        m_DeferredEffect.SetViewMatrix(m_pLightCamera->GetViewMatrixXM());
        m_DeferredEffect.SetProjMatrix(m_pLightCamera->GetProjMatrixXM(true));
        m_SkyboxEffect.SetViewMatrix(m_pLightCamera->GetViewMatrixXM());
        m_SkyboxEffect.SetProjMatrix(m_pLightCamera->GetProjMatrixXM(true));
    }
    else 
    {
        // Reversed - Z
        XMMATRIX ShadowProjRZ = m_CSManager.GetShadowProjectionXM(
            static_cast<int>(m_CSManager.m_SelectedCamera) - 2);
        ShadowProjRZ.r[2] *= g_XMNegateZ.v;
        ShadowProjRZ.r[3] = XMVectorSetZ(ShadowProjRZ.r[3], 1.0f - XMVectorGetZ(ShadowProjRZ.r[3]));

        m_DeferredEffect.SetViewMatrix(m_pLightCamera->GetViewMatrixXM());
        m_DeferredEffect.SetProjMatrix(ShadowProjRZ);
        m_SkyboxEffect.SetViewMatrix(m_pLightCamera->GetViewMatrixXM());
        m_SkyboxEffect.SetProjMatrix(ShadowProjRZ);
    }
    m_ShadowEffect.SetViewMatrix(m_pLightCamera->GetViewMatrixXM());
    m_SSAOEffect.SetViewMatrix(m_pCamera->GetViewMatrixXM());
    m_SSAOEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));

    
    if (m_UseIBL)
        m_CSManager.UpdateFrame(*m_pCamera, *m_pLightCamera, m_Test.GetModel()->boundingbox);
    else
        m_CSManager.UpdateFrame(*m_pCamera, *m_pLightCamera, m_Sponza.GetModel()->boundingbox);

    UpdateLights(dt * m_timeScale);
}

void GameApp::DrawScene()
{
	assert(m_pd3dImmediateContext);
	assert(m_pSwapChain);

    // BackBufferのRTVを作成する
	if (m_FrameCount < m_BackBufferCount)
	{
		ComPtr<ID3D11Texture2D> pBackBuffer;
		m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(pBackBuffer.GetAddressOf()));
		CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		m_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), &rtvDesc, m_pRenderTargetViews[m_FrameCount].ReleaseAndGetAddressOf());
	}

	RenderShadowForAllCascades();
	RenderGBuffer();
	RenderSSAO();

	m_DeferredEffect.SetCascadeFrustumsEyeSpaceDepths(m_CSManager.GetCascadePartitions());

    // NDC空間[-1, +1]^2をテクスチャ座標空間[0, 1]^2に変換する
	static XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	XMFLOAT4 scales[4]{};
	XMFLOAT4 offsets[4]{};
	for (size_t cascadeIndex = 0; cascadeIndex < m_CSManager.m_CascadeLevels; ++cascadeIndex)
	{
		XMMATRIX ShadowTexture = m_CSManager.GetShadowProjectionXM(cascadeIndex) * T;
		scales[cascadeIndex].x = XMVectorGetX(ShadowTexture.r[0]);
		scales[cascadeIndex].y = XMVectorGetY(ShadowTexture.r[1]);
		scales[cascadeIndex].z = XMVectorGetZ(ShadowTexture.r[2]);
		scales[cascadeIndex].w = 1.0f;
		XMStoreFloat3((XMFLOAT3*)(offsets + cascadeIndex), ShadowTexture.r[3]);
	}
	m_DeferredEffect.SetCascadeOffsets(offsets);
	m_DeferredEffect.SetCascadeScales(scales);
	m_DeferredEffect.SetShadowViewMatrix(m_pLightCamera->GetViewMatrixXM());

	m_GpuTimer_LightCulling.Start();
	{
		m_DeferredEffect.ComputeTiledLightCulling(m_pd3dImmediateContext.Get(),
			m_pFlatLitBuffer->GetUnorderedAccess(),
			m_pPointLightBuffer->GetShaderResource(),
			m_pGBufferSRVs.data(),
			m_SSAOManager.GetAmbientOcclusionTexture(),
			m_CSManager.GetCascadesOutput(),
			m_IBLManager.GetIrradianceTextureCubeSRV(),
			m_IBLManager.GetSkyboxTextureCubeSRV(),
			m_IBLManager.GetBRDFLUTTextureSRV());
	}
	m_GpuTimer_LightCulling.Stop();

	RenderSkybox();
	DrawImGui();

	HR(m_pSwapChain->Present(0, m_IsDxgiFlipModel ? DXGI_PRESENT_ALLOW_TEARING : 0));
}


bool GameApp::InitResource()
{
    // ******************
    // カメラ初期化
    //
    m_pCamera = std::make_shared<FirstPersonCamera>();

    m_pCamera->SetViewPort(0.0f, 0.0f, (float)m_ClientWidth, (float)m_ClientHeight);
    m_pCamera->SetFrustum(XM_PI / 3, AspectRatio(), 0.5f, 300.0f);
    m_pCamera->LookAt(XMFLOAT3(-6.0f, 4.0f, 4.0f), XMFLOAT3(4.0f, 4.0f, 4.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
    
    m_CameraController.InitCamera(m_pCamera.get());
    m_CameraController.SetMoveSpeed(15.0f);

    // ディレクショナルライト
    m_pLightCamera = std::make_shared<FirstPersonCamera>();

    m_pLightCamera->SetViewPort(0.0f, 0.0f, (float)m_ClientWidth, (float)m_ClientHeight);
    m_pLightCamera->LookAt(XMFLOAT3(-320.0f, 300.0f, -220.3f), XMFLOAT3(), XMFLOAT3(0.0f, 1.0f, 0.0f));
    m_pLightCamera->SetFrustum(XM_PI / 3, 1.0f, 0.1f, 1000.0f);

    m_DirLightColor = XMFLOAT3(1.0f, 0.956862f, 0.839215f);
    m_DirLightIntensity = XM_PI;
    

    // Load HDR from file
    m_TextureManager.AddTexture("EnvironmentMap_HDR", m_TextureManager.CreateFromFile("..\\Texture\\kloppenheim_06_puresky_4k.hdr"));

    // スガイボックスのように立方体の設定
    {
        Model* pModel = m_ModelManager.CreateFromGeometry("ToolCameraObject", Geometry::CreateBox());
        pModel->SetDebugObjectName("ToolCameraObject");
        pModel->materials[0].Set<std::string>("$EnvironmentMap_HDR", "EnvironmentMap_HDR");
        m_IBLManager.GetRenderGameObject()->SetModel(pModel);
    }

    // player texture model setting
    {
        m_TextureManager.AddTexture("albedo", m_TextureManager.CreateFromFile("..\\Texture\\4K_sphere\\rust_sphere_albedo.png"));
        m_TextureManager.AddTexture("metallic", m_TextureManager.CreateFromFile("..\\Texture\\4K_sphere\\rust_sphere_metallic.png"));
        m_TextureManager.AddTexture("normal", m_TextureManager.CreateFromFile("..\\Texture\\4K_sphere\\Metal055B_4K-PNG_NormalDX.png"));
        m_TextureManager.AddTexture("roughness", m_TextureManager.CreateFromFile("..\\Texture\\4K_sphere\\rust_sphere_roughness.png"));
        m_TextureManager.AddTexture("displacement", m_TextureManager.CreateFromFile("..\\Texture\\4K_sphere\\Metal055B_4K-PNG_Displacement.png"));

        Model* pModel = m_ModelManager.CreateFromGeometry("Player", Geometry::CreateSphere(6.0f, 60u, 60u));
        pModel->SetDebugObjectName("Player");
        pModel->materials[0].Set<std::string>("$Albedo", "albedo");
        pModel->materials[0].Set<std::string>("$Metallic", "metallic");
        pModel->materials[0].Set<std::string>("$Normal", "normal");
        pModel->materials[0].Set<std::string>("$Roughness", "roughness");
        //pModel->materials[0].Set<std::string>("$AO", "ao");
        pModel->materials[0].Set<std::string>("$Displacement", "displacement");
        m_Test.SetModel(pModel);
        m_Test.GetTransform().SetPosition(XMFLOAT3(0.0f, 10.0f, 0.0f));
    }

    // sponza
    {
        Model* pModel = m_ModelManager.CreateFromFile("..\\Model\\SponzaPBR\\sponza.gltf");
        // 問題のあるサブモデルを削除する
        pModel->meshdatas.erase(std::remove_if(pModel->meshdatas.begin(), pModel->meshdatas.end(), [](const MeshData& meshData) {
            return meshData.m_IndexCount == 54;
            }));
        m_Sponza.SetModel(pModel);
        m_Sponza.GetTransform().SetScale(0.05f, 0.05f, 0.05f);
    }

    m_IBLManager.InitResource(m_pd3dDevice.Get());
    m_IBLManager.RenderHDRtoCubeMaps(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(), m_IBLEffect);
    m_IBLManager.RenderIrradianceCubeMaps(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(), m_IBLEffect);
    m_IBLManager.RenderPrefilterMaps(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(), m_IBLEffect);
    m_IBLManager.RenderBRDFLUT(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(), m_IBLEffect);

    // Print BRDF-LUT
    //SaveDDSTextureToFile(m_pd3dImmediateContext.Get(), m_IBLManager.GetBRDFLUTTexture(), L"..\\Texture\\BRDFLUT.dds");

    // Skybox
    {
        m_TextureManager.AddTexture("SkyboxTextureCube", m_IBLManager.GetSkyboxTextureCubeSRV());
        m_ModelManager.CreateFromGeometry("skyboxCube", Geometry::CreateBox());
        Model* pModel = m_ModelManager.GetModel("skyboxCube");
        pModel->materials[0].Set<std::string>("$Skybox", "SkyboxTextureCube");
        m_Skybox.SetModel(pModel);
    }


    // ******************
    // Lighting初期化
    //
    InitLightParams();
    ResizeLights(m_ActiveLights);
    UpdateLights(0.0f);

    // ******************
    // Effect初期化
    //
    m_DeferredEffect.SetViewMatrix(m_pCamera->GetViewMatrixXM());
    m_DeferredEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));
    m_DeferredEffect.SetCameraNearFar(0.5f, 300.0f);
    m_DeferredEffect.SetMsaaSamples(1);

    m_DeferredEffect.SetShadowType(static_cast<int>(m_CSManager.m_ShadowType));

    m_DeferredEffect.SetPCFKernelSize(m_CSManager.m_PCFKernelSize);
    m_DeferredEffect.SetPCFDepthBias(m_CSManager.m_PCFDepthBias);
    m_DeferredEffect.SetShadowSize(m_CSManager.m_ShadowSize);

    m_DeferredEffect.SetCascadeBlendArea(m_CSManager.m_BlendBetweenCascadesRange);
    m_DeferredEffect.SetCascadeLevels(m_CSManager.m_CascadeLevels);
    m_DeferredEffect.SetCascadeIntervalSelectionEnabled(static_cast<bool>(m_CSManager.m_SelectedCascadeSelection));
    // VSM LightBleeding
    m_DeferredEffect.SetLightBleedingReduction(m_CSManager.m_LightBleedingReduction);
    // ESM
    m_DeferredEffect.SetMagicPower(m_CSManager.m_MagicPower);
    // EVSM
    m_DeferredEffect.SetPosExponent(m_CSManager.m_PosExp);
    m_DeferredEffect.SetNegExponent(m_CSManager.m_NegExp);

    m_ShadowEffect.SetViewMatrix(m_pLightCamera->GetViewMatrixXM());
    // Blur VSM
    m_ShadowEffect.SetShadowSize(m_CSManager.m_ShadowSize);
    m_ShadowEffect.SetBlurKernelSize(m_CSManager.m_BlurKernelSize);
    m_ShadowEffect.SetBlurSigma(m_CSManager.m_GaussianBlurSigma);

    m_SSAOEffect.SetViewMatrix(m_pCamera->GetViewMatrixXM());
    m_SSAOEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));

    m_SkyboxEffect.SetWorldMatrix(m_pCamera->GetLocalToWorldMatrixXM());
    m_SkyboxEffect.SetViewMatrix(m_pCamera->GetViewMatrixXM());
    m_SkyboxEffect.SetProjMatrix(m_pCamera->GetProjMatrixXM(true));
    m_SkyboxEffect.SetMsaaSamples(1);
    // ******************
    // ShadowManager 初期化
    //
    m_CSManager.InitResource(m_pd3dDevice.Get());
    

    // ******************
    // SSAOManager 初期化
    //
    m_SSAOManager.InitResource(m_pd3dDevice.Get(), m_ClientWidth, m_ClientHeight);
    
    return true;
}

void GameApp::InitLightParams()
{
    // PointLight
    m_PointLightParams.resize(MAX_LIGHTS);
    // PointLight Movement Data
    m_PointLightInitDatas.resize(MAX_LIGHTS);
    // PointLight Pos in Worldspace note we need to transform to viewspace
    m_PointLightPosWorlds.resize(MAX_LIGHTS);

    std::mt19937 rng(1225);
    constexpr float maxRadius = 100.0f;
    std::uniform_real_distribution<float> radiusNormDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * XM_PI);
    std::uniform_real_distribution<float> heightDist(0.0f, 75.0f);
    std::uniform_real_distribution<float> animationSpeedDist(2.0f, 20.0f);
    std::uniform_int_distribution<int> animationDirection(0, 1);

    std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> intensityDist(0.2f, 0.8f);
    std::uniform_real_distribution<float> attenuationDist(2.0f, 20.0f);

    for (unsigned int i = 0; i < MAX_LIGHTS; ++i)
    {
        PointLight& params = m_PointLightParams[i];
        PointLightInitData& data = m_PointLightInitDatas[i];

        data.radius = std::sqrt(radiusNormDist(rng)) * maxRadius;
        data.angle = angleDist(rng);
        data.height = heightDist(rng);
        // 速度を正規化する
        data.animationSpeed = (animationDirection(rng) * 2 - 1) * animationSpeedDist(rng) / data.radius;

        // HSL->RGB
        params.color = HueToRGB(hueDist(rng));
        XMStoreFloat3(&params.color, XMLoadFloat3(&params.color) * intensityDist(rng));
        params.range = attenuationDist(rng);
        params.intensity = intensityDist(rng) * 100;
    }

    
}

void GameApp::RenderGBuffer()
{
    // 注意：実際には深度バッファだけをクリアすれば十分。なぜなら、描画されていないピクセル（例えば遠平面）にはスカイボックスのサンプルを使うから
    //       位置の再構築には深度バッファを使い、視錐台内の位置だけがシェーディング対象になる
    // 注意：逆Zバッファを使用しており、遠平面は0である点に注意
    if (m_ClearGBuffers)
    {
        float black[4] = { 0.0f,0.0f,0.0f,1.0f };
        for (auto rtv : m_pGBufferRTVs)
            m_pd3dImmediateContext->ClearRenderTargetView(rtv, black);
    }
    m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthBuffer->GetDepthStencil(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

    D3D11_VIEWPORT viewport = m_pCamera->GetViewPort();
    m_pd3dImmediateContext->RSSetViewports(1, &viewport);

    BoundingFrustum frustum;
    BoundingFrustum::CreateFromMatrix(frustum, m_pCamera->GetProjMatrixXM());
    frustum.Transform(frustum, m_pCamera->GetLocalToWorldMatrixXM());
    if (m_UseIBL)
    {

    }
    else {
        m_Sponza.FrustumCulling(frustum);
    }

    m_GpuTimer_Geometry.Start();
    {
        m_DeferredEffect.SetRenderGBuffer(true);

        m_pd3dImmediateContext->OMSetRenderTargets(static_cast<UINT>(m_pGBuffers.size()), m_pGBufferRTVs.data(), m_pDepthBuffer->GetDepthStencil());
        if (m_UseIBL)
        {
            m_Test.Draw(m_pd3dImmediateContext.Get(), m_DeferredEffect);
        }
        else
        {
            m_Sponza.Draw(m_pd3dImmediateContext.Get(), m_DeferredEffect);
        }
            

        m_pd3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    }
    m_GpuTimer_Geometry.Stop();
    
    
}

void GameApp::RenderSkybox()
{
    m_GpuTimer_Skybox.Start();
    {
        D3D11_VIEWPORT skyboxViewport = m_pCamera->GetViewPort();
        skyboxViewport.MinDepth = 1.0f;
        skyboxViewport.MaxDepth = 1.0f;
        m_pd3dImmediateContext->RSSetViewports(1, &skyboxViewport);
        m_SkyboxEffect.SetSSAOTexture(m_SSAOManager.GetAmbientOcclusionTexture());

        switch (m_ToneMapping)
        {
        case SkyboxToneMapEffect::ToneMapping_Reinhard:
            m_SkyboxEffect.SetRenderStandard();
            break;
        case SkyboxToneMapEffect::ToneMapping_Standard:
            m_SkyboxEffect.SetRenderDefault();
            break;
        case SkyboxToneMapEffect::ToneMapping_ACES:
            m_SkyboxEffect.SetRenderACES();
            break;
        case SkyboxToneMapEffect::ToneMapping_ACES_Coarse:
            m_SkyboxEffect.SetRenderACES_COARSE();
            break;
        default:
            break;
        }

        m_SkyboxEffect.SetFlatLitTexture(m_pFlatLitBuffer->GetShaderResource(), m_ClientWidth, m_ClientHeight);
        m_SkyboxEffect.SetDepthTexture(m_pDepthBuffer->GetShaderResource());
        m_SkyboxEffect.Apply(m_pd3dImmediateContext.Get());

        // 全画面描画のため、深度バッファは使用しないので、バックバッファのクリアは必要ありません。
        ID3D11RenderTargetView* pRTVs[] = { GetBackBufferRTV() };
        m_pd3dImmediateContext->OMSetRenderTargets(1, pRTVs, nullptr);
        m_Skybox.Draw(m_pd3dImmediateContext.Get(), m_SkyboxEffect);

        // ステータスをリセットする
        m_pd3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_SkyboxEffect.SetLitTexture(nullptr);
        m_SkyboxEffect.SetDepthTexture(nullptr);
        m_SkyboxEffect.SetFlatLitTexture(nullptr, 0, 0);
        m_SkyboxEffect.SetSSAOTexture(nullptr);
        m_SkyboxEffect.Apply(m_pd3dImmediateContext.Get());
    }
    m_GpuTimer_Skybox.Stop();
}

void GameApp::ResizeBuffers(UINT width, UINT height, UINT msaaSamples)
{
    UINT tileWidth = (width + COMPUTE_SHADER_TILE_GROUP_DIM - 1) / COMPUTE_SHADER_TILE_GROUP_DIM;
    UINT tileHeight = (height + COMPUTE_SHADER_TILE_GROUP_DIM - 1) / COMPUTE_SHADER_TILE_GROUP_DIM;

    // ******************
    // DeferredResource初期化
    //
    DXGI_SAMPLE_DESC sampleDesc{};
    sampleDesc.Count = msaaSamples;
    sampleDesc.Quality = 0;
    m_pLitBuffer = std::make_unique<Texture2DMS>(m_pd3dDevice.Get(), width, height,
        DXGI_FORMAT_R16G16B16A16_FLOAT, sampleDesc,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    m_pDepthBuffer = std::make_unique<Depth2DMS>(m_pd3dDevice.Get(), width, height, sampleDesc,
        // MSAAを使用する場合、テンプレートを提供する必要があります
        m_MsaaSamples > 1 ? DepthStencilBitsFlag::Depth_32Bits_Stencil_8Bits_Unused_24Bits : DepthStencilBitsFlag::Depth_32Bits,
        D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);

    m_pFlatLitBuffer = std::make_unique<StructuredBuffer<DirectX::XMUINT2>>(
        m_pd3dDevice.Get(), width * height * msaaSamples,
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);
    
    // 読み取り専用の深度/テンプレートビューを作成
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        m_pDepthBuffer->GetDepthStencil()->GetDesc(&desc);
        desc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
        m_pd3dDevice->CreateDepthStencilView(m_pDepthBuffer->GetTexture(), &desc, m_pDepthBufferReadOnlyDSV.GetAddressOf());
    }

    // G-Buffer
    // MRTでは、すべてのG-Bufferが同じMSAAサンプリングレベルを使用する必要があります
    m_pGBuffers.clear();
    // normal + roughness + metallic
    m_pGBuffers.push_back(std::make_unique<Texture2DMS>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, sampleDesc,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
    // albedo
    m_pGBuffers.push_back(std::make_unique<Texture2DMS>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM, sampleDesc,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));
    // posZgrad
    m_pGBuffers.push_back(std::make_unique<Texture2DMS>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R16G16_FLOAT, sampleDesc,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE));

    // GBufferリソースリストを設定
    m_pGBufferRTVs.resize(m_pGBuffers.size(), 0);
    m_pGBufferSRVs.resize(4, 0);
    for (std::size_t i = 0; i < m_pGBuffers.size(); ++i) {
        m_pGBufferRTVs[i] = m_pGBuffers[i]->GetRenderTarget();
        m_pGBufferSRVs[i] = m_pGBuffers[i]->GetShaderResource();
    }
    // 最後のSRVとして深度バッファを読み取り用に設定
    m_pGBufferSRVs.back() = m_pDepthBuffer->GetShaderResource();

    // デバッグ用バッファ
    m_pDebugNormalBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_pDebugAlbedoBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_pDebugPosZGradBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R16G16_FLOAT);
    m_pDebugMetallicBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_pDebugRoughnessBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_pDebugAOTexture = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width / 2, height / 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_pDebugDepthHBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    m_pDebugShadowBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), m_CSManager.m_ShadowSize, m_CSManager.m_ShadowSize, DXGI_FORMAT_R8G8B8A8_UNORM);

    // ******************
    // デバッグ用オブジェクト名を設定
    //
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    SetDebugObjectName(m_pDepthBufferReadOnlyDSV.Get(), "DepthBufferReadOnlyDSV");
    m_pDepthBuffer->SetDebugObjectName("DepthBuffer");
    m_pLitBuffer->SetDebugObjectName("LitBuffer");
    m_pGBuffers[0]->SetDebugObjectName("GBuffer_Normal_Specular");
    m_pGBuffers[1]->SetDebugObjectName("GBuffer_Albedo");
    m_pGBuffers[2]->SetDebugObjectName("GBuffer_PosZgrad");
    m_pFlatLitBuffer->SetDebugObjectName("FlatLitBuffer");
    m_pDebugAOTexture->SetDebugObjectName("DebugAOTexture");
    m_pDebugDepthHBuffer->SetDebugObjectName("DebugDepthHBuffer");
    m_pDebugShadowBuffer->SetDebugObjectName("DebugShadowBuffer");
#endif
}

void GameApp::UpdateLights(float dt)
{
    static float totalTime = 0.0f;
    totalTime += dt;
    for (UINT i = 0; i < m_ActiveLights; ++i)
    {
        const auto& data = m_PointLightInitDatas[i];
        float angle = data.angle + totalTime * data.animationSpeed;
        m_PointLightPosWorlds[i] = XMFLOAT3(
            data.radius * std::cos(angle),
            data.height * m_LightHeightScale,
            data.radius * std::sin(angle)
        );
    }

    // ディレクショナルライトの更新
    XMFLOAT3 dirLightDir = m_pLightCamera->GetLookAxis();
    XMStoreFloat3(&dirLightDir, XMVector3TransformNormal(XMLoadFloat3(&dirLightDir), m_pCamera->GetViewMatrixXM()));
    m_DeferredEffect.SetDirectionalLight(dirLightDir, m_DirLightColor, m_DirLightIntensity);

    // ライトの位置をカメラ空間に変換
    XMVector3TransformCoordStream(&m_PointLightParams[0].position, sizeof(PointLight),
        &m_PointLightPosWorlds[0], sizeof(XMFLOAT3), m_ActiveLights, m_pCamera->GetViewMatrixXM());
    // 不要なポイントライトのデータをクリアする
    PointLight* pData = m_pPointLightBuffer->MapDiscard(m_pd3dImmediateContext.Get());
    // ポイントライトを更新する
    memcpy_s(pData, sizeof(PointLight) * m_ActiveLights,
        m_PointLightParams.data(), sizeof(PointLight) * m_ActiveLights);
    m_pPointLightBuffer->Unmap(m_pd3dImmediateContext.Get());
}

void GameApp::UpdateImGui(float dt)
{
    bool need_gpu_timer_reset = false;
#pragma region TBDR
    if (ImGui::Begin("TBDR"))
    {
        static const char* msaa_modes[] = {
            "None",
            "2x MSAA",
            "4x MSAA",
            "8x MSAA"
        };
        static int curr_msaa_item = 0;
        if (ImGui::Combo("MSAA", &curr_msaa_item, msaa_modes, ARRAYSIZE(msaa_modes)))
        {
            switch (curr_msaa_item)
            {
            case 0: m_MsaaSamples = 1; break;
            case 1: m_MsaaSamples = 2; break;
            case 2: m_MsaaSamples = 4; break;
            case 3: m_MsaaSamples = 8; break;
            }
            ResizeBuffers(m_ClientWidth, m_ClientHeight, m_MsaaSamples);
            m_DeferredEffect.SetMsaaSamples(m_MsaaSamples);
            m_SkyboxEffect.SetMsaaSamples(m_MsaaSamples);
            need_gpu_timer_reset = true;
        }

        if (ImGui::Checkbox("Visualize Light Count", &m_VisualizeLightCount))
        { 
            m_DeferredEffect.SetVisualizeLightCount(m_VisualizeLightCount);
            need_gpu_timer_reset = true;
        }


        ImGui::Checkbox("Clear G-Buffers", &m_ClearGBuffers);
        if (m_MsaaSamples > 1 && ImGui::Checkbox("Visualize Shading Freq", &m_VisualizeShadingFreq))
        {
            m_DeferredEffect.SetVisualizeShadingFreq(m_VisualizeShadingFreq);
            need_gpu_timer_reset = true;
        }
        

        ImGui::Text("Light Height Scale");
        ImGui::PushID(0);
        if (ImGui::SliderFloat("", &m_LightHeightScale, 0.25f, 1.0f))
        {
            UpdateLights(0.0f);
            need_gpu_timer_reset = true;
        }
        ImGui::PopID();

        ImGui::Text("Light Time Scale");
        ImGui::PushID(1);
        if (ImGui::SliderFloat("", &m_timeScale, 0.0f, 1.0f))
        {
            UpdateLights(0.0f);
            need_gpu_timer_reset = true;
        }
        ImGui::PopID();

        static int light_level = static_cast<int>(log2f(static_cast<float>(m_ActiveLights)));
        ImGui::Text("Lights: %d", m_ActiveLights);
        ImGui::PushID(2);
        if (ImGui::SliderInt("", &light_level, 0, (int)roundf(log2f(MAX_LIGHTS)), ""))
        {
            m_ActiveLights = (1 << light_level);
            ResizeLights(m_ActiveLights);
            UpdateLights(0.0f);
            need_gpu_timer_reset = true;
        }
        ImGui::PopID();

        if (ImGui::CollapsingHeader("Settings"))
        {
            ImGui::Checkbox("Debug Normal", &m_DebugNormal);
            ImGui::Checkbox("Debug Albedo", &m_DebugAlbedo);
            ImGui::Checkbox("Debug Roughness", &m_DebugRoughness);
            ImGui::Checkbox("Debug Metallic", &m_DebugMetallic);
            ImGui::Checkbox("Debug PosZGrad", &m_DebugPosZGrad);
            
        }
    }
    ImGui::End();
#pragma endregion TBDR

#pragma region SHADOW
    if (ImGui::Begin("Cascaded Shadow Mapping"))
    {
        static const char* shadow_modes[] = {
            "CSM",
            "VSM",
            "ESM",
            "EVSM2",
            "EVSM4"
        };
        if (ImGui::Combo("Shadow Type", reinterpret_cast<int*>(&m_CSManager.m_ShadowType), shadow_modes, ARRAYSIZE(shadow_modes)))
        {
            m_DeferredEffect.SetShadowType(static_cast<int>(m_CSManager.m_ShadowType));
            m_CSManager.InitResource(m_pd3dDevice.Get());
            need_gpu_timer_reset = true;
        }

        ImGui::Checkbox("Debug Shadow", &m_DebugShadow);

        static bool visualizeCascades = false;
        if (ImGui::Checkbox("Visualize Cascades", &visualizeCascades))
        {
            m_DeferredEffect.SetCascadeVisulization(visualizeCascades);
            need_gpu_timer_reset = true;
        }

        static int texture_level = 10;
        ImGui::Text("Texture Size: %d", m_CSManager.m_ShadowSize);
        if (ImGui::SliderInt("##0", &texture_level, 9, 12, ""))
        {
            m_CSManager.m_ShadowSize = (1 << texture_level);
            m_CSManager.InitResource(m_pd3dDevice.Get());
            m_DeferredEffect.SetShadowSize(m_CSManager.m_ShadowSize);
            m_ShadowEffect.SetShadowSize(m_CSManager.m_ShadowSize);
            m_pDebugShadowBuffer = std::make_unique<Texture2D>(m_pd3dDevice.Get(), m_CSManager.m_ShadowSize, m_CSManager.m_ShadowSize, DXGI_FORMAT_R8G8B8A8_UNORM);
            need_gpu_timer_reset = true;
        }

        static int blur_size = 2;
        ImGui::Text("Blur Size: %d", m_CSManager.m_BlurKernelSize);
        if (ImGui::SliderInt("##2", &blur_size, 0, 7, ""))
        {
            m_CSManager.m_BlurKernelSize = 2 * blur_size + 1;
            m_DeferredEffect.SetPCFKernelSize(m_CSManager.m_BlurKernelSize);
            m_ShadowEffect.SetBlurKernelSize(m_CSManager.m_BlurKernelSize);
            need_gpu_timer_reset = true;
        }

        if (m_CSManager.m_ShadowType >= ShadowType::ShadowType_VSM)
        {
            if (ImGui::SliderFloat("Blur Sigma", &m_CSManager.m_GaussianBlurSigma, 0.1f, 10.0f, "%.1f"))
            {
                m_ShadowEffect.SetBlurSigma(m_CSManager.m_GaussianBlurSigma);
            }
        }

        if (m_CSManager.m_ShadowType == ShadowType::ShadowType_CSM && ImGui::SliderFloat("Depth Bias", &m_CSManager.m_PCFDepthBias, 0.0f, 0.05f))
        {
            m_DeferredEffect.SetPCFDepthBias(m_CSManager.m_PCFDepthBias);
        }

        if (m_CSManager.m_ShadowType == ShadowType::ShadowType_VSM || m_CSManager.m_ShadowType >= ShadowType::ShadowType_EVSM2)
        {
            if (ImGui::SliderFloat("Light Bleeding", &m_CSManager.m_LightBleedingReduction, 0.0f, 1.0f, "%.2f"))
            {
                m_DeferredEffect.SetLightBleedingReduction(m_CSManager.m_LightBleedingReduction);
            }
        }

        if (m_CSManager.m_ShadowType == ShadowType::ShadowType_ESM)
        {
            if (ImGui::SliderFloat("Magic Power", &m_CSManager.m_MagicPower, 0.1f, 400.0f, "%.1f"))
            {
                m_DeferredEffect.SetMagicPower(m_CSManager.m_MagicPower);
            }
        }

        if (m_CSManager.m_ShadowType >= ShadowType::ShadowType_EVSM2)
        {
            if (ImGui::SliderFloat("Pos Exp", &m_CSManager.m_PosExp, 0.1f, 42.0f, "%.1f"))
            {
                m_DeferredEffect.SetPosExponent(m_CSManager.m_PosExp);
            }
        }

        if (m_CSManager.m_ShadowType == ShadowType::ShadowType_EVSM4)
        {
            if (ImGui::SliderFloat("Neg Exp", &m_CSManager.m_NegExp, 0.1f, 42.0f, "%.1f"))
            {
                m_DeferredEffect.SetNegExponent(m_CSManager.m_NegExp);
            }
        }

        ImGui::Text("Cascade Blur");
        if (ImGui::SliderFloat("##3", &m_CSManager.m_BlendBetweenCascadesRange, 0.0f, 0.5f))
        {
            m_DeferredEffect.SetCascadeBlendArea(m_CSManager.m_BlendBetweenCascadesRange);
        }

        if (ImGui::Checkbox("Fixed Size Frustum AABB", &m_CSManager.m_FixedSizeFrustumAABB))
            need_gpu_timer_reset = true;

        if (ImGui::Checkbox("Fit Light to Texels", &m_CSManager.m_MoveLightTexelSize))
            need_gpu_timer_reset = true;

        static const char* fit_projection_strs[] = {
            "Fit Projection To Cascade",
            "Fit Projection To Scene"
        };
        if (ImGui::Combo("##4", reinterpret_cast<int*>(&m_CSManager.m_SelectedCascadesFit), fit_projection_strs, ARRAYSIZE(fit_projection_strs)))
            need_gpu_timer_reset = true;

        static const char* camera_strs[] = {
            "Main Camera",
            "Light Camera",
            "Cascade Camera 1",
            "Cascade Camera 2",
            "Cascade Camera 3",
            "Cascade Camera 4",
        };
        static int camera_idx = 0;
        if (camera_idx > m_CSManager.m_CascadeLevels + 2)
            camera_idx = m_CSManager.m_CascadeLevels + 2;
        if (ImGui::Combo("##5", &camera_idx, camera_strs, m_CSManager.m_CascadeLevels + 2))
        {
            m_CSManager.m_SelectedCamera = static_cast<CameraSelection>(camera_idx);
            if (m_CSManager.m_SelectedCamera == CameraSelection::CameraSelection_Eye)
            {
                m_CameraController.InitCamera(static_cast<FirstPersonCamera*>(m_pCamera.get()));
                m_CameraController.SetMoveSpeed(15.0f);
            }
            else if (m_CSManager.m_SelectedCamera == CameraSelection::CameraSelection_Light)
            {
                m_CameraController.InitCamera(static_cast<FirstPersonCamera*>(m_pLightCamera.get()));
                m_CameraController.SetMoveSpeed(50.0f);
            }
        }

        static const char* fit_near_far_strs[] = {
            "0:1 NearFar",
            "Cascade AABB NearFar",
            "Scene AABB NearFar",
            "Scene AABB Intersection NearFar"
        };
        if (ImGui::Combo("##6", reinterpret_cast<int*>(&m_CSManager.m_SelectedNearFarFit), fit_near_far_strs, ARRAYSIZE(fit_near_far_strs)))
        {
            need_gpu_timer_reset = true;
        }

        static const char* cascade_selection_strs[] = {
            "Map-based Selection",
            "Interval-based Selection",
        };
        if (ImGui::Combo("##7", reinterpret_cast<int*>(&m_CSManager.m_SelectedCascadeSelection), cascade_selection_strs, ARRAYSIZE(cascade_selection_strs)))
        {
            m_DeferredEffect.SetCascadeIntervalSelectionEnabled(static_cast<bool>(m_CSManager.m_SelectedCascadeSelection));
            need_gpu_timer_reset = true;
        }

        static const char* cascade_levels[] = {
            "1 Level",
            "2 Levels",
            "3 Levels",
            "4 Levels"
        };
        static int cascade_level_idx = m_CSManager.m_CascadeLevels - 1;
        if (ImGui::Combo("Cascade", &cascade_level_idx, cascade_levels, ARRAYSIZE(cascade_levels)))
        {
            m_CSManager.m_CascadeLevels = cascade_level_idx + 1;
            m_CSManager.InitResource(m_pd3dDevice.Get());
            m_DeferredEffect.SetCascadeLevels(m_CSManager.m_CascadeLevels);
            need_gpu_timer_reset = true;
        }

        char level_str[] = "level1";
        for (int i = 0; i < m_CSManager.m_CascadeLevels; ++i)
        {
            level_str[5] = '1' + i;
            ImGui::SliderFloat(level_str, m_CSManager.m_CascadePartitionsPercentage + i, 0.0f, 1.0f, "");
            ImGui::SameLine();
            ImGui::Text("%.1f%%", m_CSManager.m_CascadePartitionsPercentage[i] * 100);
            if (i && m_CSManager.m_CascadePartitionsPercentage[i] < m_CSManager.m_CascadePartitionsPercentage[i - 1])
                m_CSManager.m_CascadePartitionsPercentage[i] = m_CSManager.m_CascadePartitionsPercentage[i - 1];
            if (i < m_CSManager.m_CascadeLevels - 1 && m_CSManager.m_CascadePartitionsPercentage[i] > m_CSManager.m_CascadePartitionsPercentage[i + 1])
                m_CSManager.m_CascadePartitionsPercentage[i] = m_CSManager.m_CascadePartitionsPercentage[i + 1];
            if (m_CSManager.m_CascadePartitionsPercentage[i] > 1.0f)
                m_CSManager.m_CascadePartitionsPercentage[i] = 1.0f;
        }

        if (ImGui::CollapsingHeader("Directional Light"))
        {
            static XMFLOAT3 rotationAngle = XMFLOAT3(0.0f, 0.0f, 0.0f);
            if (ImGui::InputFloat3("Euler Angles", reinterpret_cast<float*>(&rotationAngle)))
                m_pLightCamera->SetRotation(rotationAngle.x / 180 * XM_PI, rotationAngle.y / 180 * XM_PI, rotationAngle.z / 180 * XM_PI);
            ImGui::ColorEdit3("Color##dirLight", reinterpret_cast<float*>(&m_DirLightColor));
            ImGui::InputFloat("Intensity##dirLight", &m_DirLightIntensity);
            if (m_DirLightIntensity < 0.0f)
                m_DirLightIntensity = 0.0f;
        }
    }
    ImGui::End();
#pragma endregion SHADOW

#pragma region SSAO
    if (ImGui::Begin("SSAO"))
    {

        if (!m_EnableSSAO)
            m_DebugSSAO = false;

        if (m_EnableSSAO)
        {
            ImGui::SliderFloat("Epsilon", &m_SSAOManager.m_SurfaceEpsilon, 0.0f, 0.1f, "%.2f");
            static float range = m_SSAOManager.m_OcclusionFadeEnd - m_SSAOManager.m_OcclusionFadeStart;
            ImGui::SliderFloat("Fade Start", &m_SSAOManager.m_OcclusionFadeStart, 0.0f, 2.0f, "%.2f");
            if (ImGui::SliderFloat("Fade Range", &range, 0.0f, 3.0f, "%.2f"))
            {
                m_SSAOManager.m_OcclusionFadeEnd = m_SSAOManager.m_OcclusionFadeStart + range;
            }
            ImGui::SliderFloat("Sample Radius", &m_SSAOManager.m_OcclusionRadius, 0.0f, 2.0f, "%.1f");
            ImGui::SliderInt("Sample Count", reinterpret_cast<int*>(&m_SSAOManager.m_SampleCount), 1, 14);
            ImGui::Checkbox("Debug SSAO", &m_DebugSSAO);
        }
    }
    ImGui::End();
#pragma endregion SSAO

#pragma region SKYBOXTONEMAP
    if (ImGui::Begin("SkyboxToneMap"))
    {
        if (ImGui::Checkbox("Use IBL Scene", &m_UseIBL))
        {
            if (m_UseIBL)
            {
                m_DeferredEffect.SetUseIBL(m_UseIBL);
                m_CSManager.m_SelectedNearFarFit = FitNearFar::FitNearFar_ZeroOne;
                
                m_DeferredEffect.SetCascadeVisulization(false);

                m_pCamera->LookAt(XMFLOAT3(0.0f, 10.0f, -30.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f));
                m_ActiveLights = 1;
                ResizeLights(m_ActiveLights);
                UpdateLights(0.0f);
                m_DirLightIntensity = 0.0f;
                need_gpu_timer_reset = true;
            }
            else 
            {
                m_DeferredEffect.SetUseIBL(m_UseIBL);
                m_CSManager.m_SelectedNearFarFit = FitNearFar::FitNearFar_SceneAABB_Intersection;
                m_ActiveLights = 512;
                ResizeLights(m_ActiveLights);
                UpdateLights(0.0f);
                need_gpu_timer_reset = true;
            }
            
        }

        // ToneMapping
        static const char* strs[] = {
            "Reinhard",
            "Standard",
            "ACES",
            "ACES Coarse",
        };
        ImGui::Combo("Tone Mapping", (int*)&m_ToneMapping, strs, ARRAYSIZE(strs));
    }
    ImGui::End();
#pragma endregion SKYBOXTONEMAP

    if (need_gpu_timer_reset)
    {
        m_GpuTimer_Geometry.Reset(m_pd3dImmediateContext.Get());
        m_GpuTimer_Shadow.Reset(m_pd3dImmediateContext.Get());
        m_GpuTimer_SSAO.Reset(m_pd3dImmediateContext.Get());
        m_GpuTimer_LightCulling.Reset(m_pd3dImmediateContext.Get());
        m_GpuTimer_Skybox.Reset(m_pd3dImmediateContext.Get());
    }
}

void GameApp::RenderShadowForAllCascades()
{
    m_GpuTimer_Shadow.Start();
    {
        //
        // ShadowMapを描画
        //

        // Viewport設定
        D3D11_VIEWPORT vp = m_CSManager.GetShadowViewport();
        m_pd3dImmediateContext->RSSetViewports(1, &vp);
        float clearColor[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
        // すべてのカスケードレベルを走査する
        for (size_t cascadeIdx = 0; cascadeIdx < m_CSManager.m_CascadeLevels; ++cascadeIdx)
        {
            switch (m_CSManager.m_ShadowType)
            {
            case ShadowType::ShadowType_CSM: m_ShadowEffect.SetRenderAlphaClip(0.001f);
                break;
            case ShadowType::ShadowType_VSM:
            case ShadowType::ShadowType_ESM:
            case ShadowType::ShadowType_EVSM2:
            case ShadowType::ShadowType_EVSM4: m_ShadowEffect.SetRenderDepthOnly(); break;
            }

            ID3D11RenderTargetView* nullRTV = nullptr;
            ID3D11DepthStencilView* depthDSV = m_CSManager.GetDepthBufferDSV();
            ID3D11RenderTargetView* depthRTV = m_CSManager.GetCascadeRenderTargetView(cascadeIdx);
            m_pd3dImmediateContext->ClearRenderTargetView(depthRTV, clearColor);
            m_pd3dImmediateContext->ClearDepthStencilView(depthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
            if (m_CSManager.m_ShadowType != ShadowType::ShadowType_CSM)
                depthRTV = nullptr;
            m_pd3dImmediateContext->OMSetRenderTargets(1, &depthRTV, depthDSV);

            // 作成された lightSpace の正射影行列を設定する
            XMMATRIX shadowProj = m_CSManager.GetShadowProjectionXM(cascadeIdx);
            m_ShadowEffect.SetProjMatrix(shadowProj);

            // オブジェクトと投影キューブのクリッピングを更新する
            BoundingOrientedBox obb = m_CSManager.GetShadowOBB(cascadeIdx);
            obb.Transform(obb, m_pLightCamera->GetLocalToWorldMatrixXM());

            if (m_UseIBL)
            {
                m_Test.CubeCulling(obb);
                m_Test.Draw(m_pd3dImmediateContext.Get(), m_ShadowEffect);

            }
            else
            {
                m_Sponza.CubeCulling(obb);
                m_Sponza.Draw(m_pd3dImmediateContext.Get(), m_ShadowEffect);
            }
            m_pd3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);


            if (m_CSManager.m_ShadowType == ShadowType::ShadowType_VSM ||
                m_CSManager.m_ShadowType >= ShadowType::ShadowType_EVSM2)
            {
                if (m_CSManager.m_ShadowType == ShadowType::ShadowType_VSM)
                    m_ShadowEffect.RenderVarianceShadow(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetDepthBufferSRV(),
                        m_CSManager.GetCascadeRenderTargetView(cascadeIdx),
                        m_CSManager.GetShadowViewport());
                else
                    m_ShadowEffect.RenderExponentialVarianceShadow(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetDepthBufferSRV(),
                        m_CSManager.GetCascadeRenderTargetView(cascadeIdx),
                        m_CSManager.GetShadowViewport(), m_CSManager.m_PosExp,
                        m_CSManager.m_ShadowType == ShadowType::ShadowType_EVSM4 ? &m_CSManager.m_NegExp : nullptr);


                if (m_CSManager.m_BlurKernelSize > 1)
                {
                    m_ShadowEffect.GaussianBlurX(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetCascadeOutput(cascadeIdx),
                        m_CSManager.GetTempTextureRTV(),
                        m_CSManager.GetShadowViewport());
                    m_ShadowEffect.GaussianBlurY(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetTempTextureOutput(),
                        m_CSManager.GetCascadeRenderTargetView(cascadeIdx),
                        m_CSManager.GetShadowViewport());
                }
            }
            else if (m_CSManager.m_ShadowType == ShadowType::ShadowType_ESM)
            {
                if (m_CSManager.m_BlurKernelSize > 1)
                {
                    m_ShadowEffect.RenderExponentialShadow(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetDepthBufferSRV(),
                        m_CSManager.GetTempTextureRTV(),
                        m_CSManager.GetShadowViewport(),
                        m_CSManager.m_MagicPower);
                    m_ShadowEffect.LogGaussianBlur(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetTempTextureOutput(),
                        m_CSManager.GetCascadeRenderTargetView(cascadeIdx),
                        m_CSManager.GetShadowViewport());
                }
                else
                {
                    m_ShadowEffect.RenderExponentialShadow(m_pd3dImmediateContext.Get(),
                        m_CSManager.GetDepthBufferSRV(),
                        m_CSManager.GetCascadeRenderTargetView(cascadeIdx),
                        m_CSManager.GetShadowViewport(),
                        m_CSManager.m_MagicPower);
                }
            }
        }
    }
    m_GpuTimer_Shadow.Stop();

}

void GameApp::RenderSSAO()
{
    m_GpuTimer_SSAO.Start();
    {
        m_pd3dImmediateContext->ResolveSubresource(m_SSAOManager.GetResolveNormalTexture(), 0,
            m_pGBuffers[0]->GetTexture(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
        m_SSAOEffect.SetRenderDepthOnly();
        ID3D11DepthStencilView* depthDSV = m_SSAOManager.GetDepthBufferDSV();
        // 注意：逆Zバッファを使用しているため、遠平面は0である点に注意
        D3D11_VIEWPORT viewport = m_pCamera->GetViewPort();
        m_pd3dImmediateContext->RSSetViewports(1, &viewport);

        m_pd3dImmediateContext->ClearDepthStencilView(depthDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);
        m_pd3dImmediateContext->OMSetRenderTargets(0, nullptr, depthDSV);
        if (m_UseIBL)
        {
            m_Test.Draw(m_pd3dImmediateContext.Get(), m_SSAOEffect);
        }
        else 
        {
            m_Sponza.Draw(m_pd3dImmediateContext.Get(), m_SSAOEffect);
        }
        
        m_pd3dImmediateContext->OMSetRenderTargets(0, 0, nullptr);
        // Pass 2: AO生成
        m_SSAOManager.RenderToSSAOCS(m_pd3dImmediateContext.Get(), m_SSAOEffect,
            m_SSAOManager.GetResolveNormalSRV(),
            m_SSAOManager.GetDepthBufferSRV(),
            *m_pCamera);

        // Pass 3: Blur AO
        m_SSAOManager.BlurCS(m_pd3dImmediateContext.Get(), m_SSAOEffect);
    }
    m_GpuTimer_SSAO.Stop();
}

void GameApp::DrawImGui()
{

	if (m_DebugNormal)
	{
		if (ImGui::Begin("Normal", &m_DebugNormal))
		{
			m_DeferredEffect.DebugNormalGBuffer(m_pd3dImmediateContext.Get(), m_pDebugNormalBuffer->GetRenderTarget(),
				m_pGBufferSRVs[0], m_pCamera->GetViewPort());
			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)((winSize.x - 20) / AspectRatio(), winSize.y - 36);
			ImGui::Image(m_pDebugNormalBuffer->GetShaderResource(), ImVec2(smaller * AspectRatio(), smaller));
		}
		ImGui::End();
	}

	if (m_DebugAlbedo)
	{
		if (ImGui::Begin("Albedo", &m_DebugAlbedo))
		{
			CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)m_pDebugAlbedoBuffer->GetWidth(), (float)m_pDebugAlbedoBuffer->GetHeight());
			m_DeferredEffect.DebugAlbedoGBuffer(m_pd3dImmediateContext.Get(), m_pDebugAlbedoBuffer->GetRenderTarget(), m_pGBufferSRVs[1], vp);
			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)((winSize.x - 20) / AspectRatio(), winSize.y - 36);
			ImGui::Image(m_pDebugAlbedoBuffer->GetShaderResource(), ImVec2(smaller * AspectRatio(), smaller));
		}
		ImGui::End();
	}

	if (m_DebugRoughness)
	{
		if (ImGui::Begin("Roughness", &m_DebugRoughness))
		{
			CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)m_pDebugRoughnessBuffer->GetWidth(), (float)m_pDebugRoughnessBuffer->GetHeight());
			m_DeferredEffect.DebugRoughnessGBuffer(m_pd3dImmediateContext.Get(), m_pDebugRoughnessBuffer->GetRenderTarget(), m_pGBufferSRVs[0], vp);
			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)((winSize.x - 20) / AspectRatio(), winSize.y - 36);
			ImGui::Image(m_pDebugRoughnessBuffer->GetShaderResource(), ImVec2(smaller * AspectRatio(), smaller));
		}
		ImGui::End();
	}

	if (m_DebugMetallic)
	{
		if (ImGui::Begin("Metallic", &m_DebugMetallic))
		{
			CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)m_pDebugMetallicBuffer->GetWidth(), (float)m_pDebugMetallicBuffer->GetHeight());
			m_DeferredEffect.DebugMetallicGBuffer(m_pd3dImmediateContext.Get(), m_pDebugMetallicBuffer->GetRenderTarget(), m_pGBufferSRVs[0], vp);
			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)((winSize.x - 20) / AspectRatio(), winSize.y - 36);
			ImGui::Image(m_pDebugMetallicBuffer->GetShaderResource(), ImVec2(smaller * AspectRatio(), smaller));
		}
		ImGui::End();
	}

	if (m_DebugPosZGrad)
	{
		if (ImGui::Begin("PosZGrad", &m_DebugPosZGrad))
		{
			CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)m_pDebugPosZGradBuffer->GetWidth(), (float)m_pDebugPosZGradBuffer->GetHeight());
			m_DeferredEffect.DebugPosZGradGBuffer(m_pd3dImmediateContext.Get(), m_pDebugPosZGradBuffer->GetRenderTarget(),
				m_pGBufferSRVs[2], vp);
			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)((winSize.x - 20) / AspectRatio(), winSize.y - 36);
			ImGui::Image(m_pDebugPosZGradBuffer->GetShaderResource(), ImVec2(smaller * AspectRatio(), smaller));
		}
		ImGui::End();
	}

	if (m_DebugShadow)
	{
		if (ImGui::Begin("Debug Cascaded Shadow"))
		{
			static const char* cascade_level_strs[] = {
				"Level 1","Level 2","Level 3", "Level 4",
			};
			static int curr_level_idx = 0;
			ImGui::Combo("##1", &curr_level_idx, cascade_level_strs, m_CSManager.m_CascadeLevels);
			if (curr_level_idx >= m_CSManager.m_CascadeLevels)
				curr_level_idx = m_CSManager.m_CascadeLevels - 1;

            D3D11_VIEWPORT vp = m_CSManager.GetShadowViewport();
			m_ShadowEffect.RenderDepthToTexture(m_pd3dImmediateContext.Get(),
                m_CSManager.GetCascadeOutput(curr_level_idx),
				m_pDebugShadowBuffer->GetRenderTarget(), vp);

			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)(winSize.x - 20, winSize.y - 60);
			ImGui::Image(m_pDebugShadowBuffer->GetShaderResource(), ImVec2(smaller, smaller));
		}
		ImGui::End();
	}

	if (m_DebugSSAO)
	{
		if (ImGui::Begin("SSAO Buffer", &m_DebugSSAO))
		{
			CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)m_pDebugAOTexture->GetWidth(), (float)m_pDebugAOTexture->GetHeight());
			m_SSAOEffect.RenderAmbientOcclusionToTexture(
				m_pd3dImmediateContext.Get(),
				m_SSAOManager.GetAmbientOcclusionTexture(),
				m_pDebugAOTexture->GetRenderTarget(),
				vp);
			ImVec2 winSize = ImGui::GetWindowSize();
			float smaller = (std::min)((winSize.x - 20) / AspectRatio(), winSize.y - 36);
			ImGui::Image(m_pDebugAOTexture->GetShaderResource(), ImVec2(smaller * AspectRatio(), smaller));
		}
		ImGui::End();
	}


	if (ImGui::Begin("TBDR"))
	{
		ImGui::Separator();
		ImGui::Text("GPU Profile");
		double total_time = 0.0f;

		m_GpuTimer_Geometry.TryGetTime(nullptr);
		ImGui::Text("Geometry Pass: %.3f ms", m_GpuTimer_Geometry.AverageTime() * 1000);
		total_time += m_GpuTimer_Geometry.AverageTime();

		m_GpuTimer_LightCulling.TryGetTime(nullptr);
		ImGui::Text("Light Culling Pass: %.3f ms", m_GpuTimer_LightCulling.AverageTime() * 1000);
		total_time += m_GpuTimer_LightCulling.AverageTime();

	}
	ImGui::End();

    if (ImGui::Begin("SSAO"))
    {
        ImGui::Separator();
        ImGui::Text("GPU Profile");
        m_GpuTimer_SSAO.TryGetTime(nullptr);
        ImGui::Text("SSAO Pass: %.3f ms", m_GpuTimer_SSAO.AverageTime() * 1000);
        m_GpuTimer_SSAO.AverageTime();
    }
    ImGui::End();

    if (ImGui::Begin("Cascaded Shadow Mapping"))
    {
        ImGui::Separator();
        ImGui::Text("GPU Profile");
        m_GpuTimer_Shadow.TryGetTime(nullptr);
        ImGui::Text("CSM Pass: %.3f ms", m_GpuTimer_Shadow.AverageTime() * 1000);
        m_GpuTimer_Shadow.AverageTime();
    }
    ImGui::End();

    if (ImGui::Begin("SkyboxToneMap"))
    {
        ImGui::Separator();
        ImGui::Text("GPU Profile");
        m_GpuTimer_Skybox.TryGetTime(nullptr);
        ImGui::Text("SkyboxToneMap Pass: %.3f ms", m_GpuTimer_Skybox.AverageTime() * 1000);
        m_GpuTimer_Skybox.AverageTime();
    }
    ImGui::End();

	ImGui::Render();
	ID3D11RenderTargetView* pRTVs[] = { GetBackBufferRTV() };
	m_pd3dImmediateContext->OMSetRenderTargets(1, pRTVs, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

}

void GameApp::ResizeLights(UINT activeLights)
{
    m_ActiveLights = activeLights;
    m_pPointLightBuffer = std::make_unique<StructuredBuffer<PointLight>>(m_pd3dDevice.Get(), activeLights, D3D11_BIND_SHADER_RESOURCE, false, true);

    // ******************
    // デバッグ用オブジェクト名を設定
    //
    m_pPointLightBuffer->SetDebugObjectName("LightBuffer");
}

XMFLOAT3 GameApp::HueToRGB(float hue)
{
    float intPart;
    float fracPart = std::modf(hue * 6.0f, &intPart);
    int region = static_cast<int>(intPart);

    switch (region)
    {
    case 0: return XMFLOAT3(1.0f, fracPart, 0.0f);
    case 1: return XMFLOAT3(1.0f - fracPart, 1.0f, 0.0f);
    case 2: return XMFLOAT3(0.0f, 1.0f, fracPart);
    case 3: return XMFLOAT3(0.0f, 1.0f - fracPart, 1.0f);
    case 4: return XMFLOAT3(fracPart, 0.0f, 1.0f);
    case 5: return XMFLOAT3(1.0f, 0.0f, 1.0f - fracPart);
    }
    return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

