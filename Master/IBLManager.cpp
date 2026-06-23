#include "IBLManager.h"

using namespace DirectX;

void IBLManager::InitResource(ID3D11Device* device)
{
	// CubeMapCamera
	auto cubeCamera = std::make_shared<FirstPersonCamera>();
	cubeCamera->SetViewPort(0.0f, 0.0f, m_CubeMapWidth, m_CubeMapHeight);
	cubeCamera->SetFrustum(XM_PIDIV2, m_CubeMapWidth / m_CubeMapHeight, 1.0f, 1000.0f);
	m_pCubeCamera = cubeCamera;

	// CubeMapの六つの面を初期化する
	for (int i = 0; i < 6; i++)
	{
		m_pCubeMapSlice.push_back(std::make_unique<Texture2D>(device, (uint32_t)m_CubeMapWidth, (uint32_t)m_CubeMapHeight, DXGI_FORMAT_R16G16B16A16_FLOAT));
		m_pCubeMapSlice[i]->SetDebugObjectName("m_pCubeMapSlice" + std::to_string(i));
	}

	// TextureCube + MipMapsの初期化
	m_pSkyboxTextureCube = std::make_unique<TextureCube>(device, (uint32_t)m_CubeMapWidth, (uint32_t)m_CubeMapHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, m_MipLevel);
	m_pSkyboxTextureCube->SetDebugObjectName("m_pSkyboxTextureCube");

	// IrradianceTextureCubeの初期化
	m_pIrradianceTextureCube = std::make_unique<TextureCube>(device, 64u, 64u, DXGI_FORMAT_R16G16B16A16_FLOAT);
	m_pIrradianceTextureCube->SetDebugObjectName("m_pIrradianceTextureCube");

	// PrefilterTexturesの初期化
	for (int i = 0; i < m_MipLevel; i++)
	{
		int height = static_cast<int>(m_CubeMapHeight) >> i;
		int width = static_cast<int>(m_CubeMapWidth) >> i;
		m_pPrefilterMaps.push_back(std::make_unique<Texture2D>(device, (uint32_t)width, (uint32_t)height, DXGI_FORMAT_R16G16B16A16_FLOAT));
		m_pPrefilterMaps[i]->SetDebugObjectName("m_pPrefilterMaps" + std::to_string(i));
	}

	// BRDFLUT
	m_pBRDFLUTTexture = std::make_unique<Texture2D>(device, (uint32_t)m_CubeMapWidth, (uint32_t)m_CubeMapHeight, DXGI_FORMAT_R16G16_FLOAT);
	m_pBRDFLUTTexture->SetDebugObjectName("m_pBRDFLUTTexture");
}

void IBLManager::RenderHDRtoCubeMaps(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
	IBLEffect& iblEffect)
{

	
	iblEffect.SetProjMatrix(m_pCubeCamera->GetProjMatrixXM());

	for (int faceID = 0; faceID < 6; faceID++)
	{
		m_pCubeCamera->LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f), m_LookForwards[faceID], m_LookUps[faceID]);

		
		
		D3D11_VIEWPORT viewport = m_pCubeCamera->GetViewPort();
		deviceContext->RSSetViewports(1, &viewport);

		ID3D11RenderTargetView* pRTV[1] = { m_pCubeMapSlice[faceID]->GetRenderTarget() };
		deviceContext->OMSetRenderTargets(1, pRTV, nullptr);

		iblEffect.SetViewMatrix(m_pCubeCamera->GetViewMatrixXM());
		iblEffect.RenderHDRtoCubeMaps();

		m_Cube.Draw(deviceContext, iblEffect);

		ComPtr<ID3D11Texture2D> pTex;
		D3D11_TEXTURE2D_DESC texDesc;
		m_pCubeMapSlice[faceID]->GetShaderResource()->GetResource(reinterpret_cast<ID3D11Resource**>(pTex.ReleaseAndGetAddressOf()));
		pTex->GetDesc(&texDesc);

		deviceContext->CopySubresourceRegion(m_pSkyboxTextureCube->GetTexture(),
			D3D11CalcSubresource(0, faceID, m_MipLevel), 0, 0, 0, pTex.Get(), 0, nullptr);

		deviceContext->GenerateMips(m_pSkyboxTextureCube->GetShaderResource());
	}

}

void IBLManager::RenderIrradianceCubeMaps(ID3D11Device* device, ID3D11DeviceContext* deviceContext, 
	IBLEffect& iblEffect)
{
	iblEffect.SetProjMatrix(m_pCubeCamera->GetProjMatrixXM());

	for (int faceID = 0; faceID < 6; faceID++)
	{
		m_pCubeCamera->LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f), m_LookForwards[faceID], m_LookUps[faceID]);

		iblEffect.SetViewMatrix(m_pCubeCamera->GetViewMatrixXM());
		iblEffect.SetSkyboxTexture(m_pSkyboxTextureCube->GetShaderResource());
		iblEffect.RenderIrradianceCubeMap();

		D3D11_VIEWPORT viewport = m_pCubeCamera->GetViewPort();
		viewport.Height = 64.0f;
		viewport.Width = 64.0f;
		deviceContext->RSSetViewports(1, &viewport);

		ID3D11RenderTargetView* pRTV[1] = { m_pIrradianceTextureCube->GetRenderTarget() };
		deviceContext->OMSetRenderTargets(1, pRTV, nullptr);

		m_Cube.Draw(deviceContext, iblEffect);
	}
	

}

void IBLManager::RenderPrefilterMaps(ID3D11Device* device, ID3D11DeviceContext* deviceContext, 
	IBLEffect& iblEffect)
{
	iblEffect.SetProjMatrix(m_pCubeCamera->GetProjMatrixXM());

	for (int mipID = 0; mipID < m_MipLevel; mipID++)
	{
		D3D11_VIEWPORT viewport = m_pCubeCamera->GetViewPort();
		viewport.Width = static_cast<float>(1024 >> mipID);
		viewport.Height = static_cast<float>(1024 >> mipID);
		m_Roughness = float(mipID) / (m_MipLevel - 1);
		for (int faceID = 0; faceID < 6; faceID++)
		{
			m_pCubeCamera->LookAt(XMFLOAT3(0.0f, 0.0f, 0.0f), m_LookForwards[faceID], m_LookUps[faceID]);

			iblEffect.SetViewMatrix(m_pCubeCamera->GetViewMatrixXM());
			iblEffect.SetSkyboxTexture(m_pSkyboxTextureCube->GetShaderResource());
			iblEffect.SetRoughness(m_Roughness);
			iblEffect.RenderPrefilterEnvCubeMap();

			deviceContext->RSSetViewports(1, &viewport);
			ID3D11RenderTargetView* pRTV[1] = { m_pPrefilterMaps[mipID]->GetRenderTarget() };
			deviceContext->OMSetRenderTargets(1, pRTV, nullptr);
			m_Cube.Draw(deviceContext, iblEffect);

			deviceContext->CopySubresourceRegion(m_pSkyboxTextureCube->GetTexture(),
				D3D11CalcSubresource(mipID, faceID, 6), 0, 0, 0, m_pPrefilterMaps[mipID]->GetTexture(), 0, nullptr);
		}
	}
}

void IBLManager::RenderBRDFLUT(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
	IBLEffect& iblEffect)
{
	D3D11_VIEWPORT viewport = m_pCubeCamera->GetViewPort();
	
	iblEffect.RenderBRDFLUT(deviceContext, m_pBRDFLUTTexture->GetRenderTarget(), viewport);
}
