#pragma once
#ifndef IBLMANAGER_H
#define IBLMANAGER_H

#include <d3d11_1.h>
#include <wrl/client.h>
#include <string_view>
#include <Texture2D.h>
#include <Camera.h>
#include <CameraController.h>
#include <d3dApp.h>
#include <GameObject.h>
#include "Effects.h"

class IBLManager
{
public:
	template<class T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	IBLManager() = default;
	~IBLManager() = default;
	// コピー操作は禁止、ムーブ操作のみ許可
	IBLManager(const IBLManager&) = delete;
	IBLManager& operator=(const IBLManager&) = delete;
	IBLManager(IBLManager&&) = default;
	IBLManager& operator=(IBLManager&&) = default;

	// IBLの各生成の準備
	void InitResource(ID3D11Device* device);
	// HDRテクスチャをTextureCubeになる + 0から5までレベルのMipMaps
	void RenderHDRtoCubeMaps(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
		IBLEffect& iblEffect);
	// TextureCubeによってIrradianceCubeMapを生成する
	void RenderIrradianceCubeMaps(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
		IBLEffect& iblEffect);
	// TextureCubeによってPrefilter環境テクスチャを生成する
	void RenderPrefilterMaps(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
		IBLEffect& iblEffect);
	// BRDFLUTを生成する
	void RenderBRDFLUT(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
		IBLEffect& iblEffect);


	ID3D11ShaderResourceView* GetSkyboxTextureCubeSRV()const { return m_pSkyboxTextureCube->GetShaderResource(); }
	ID3D11ShaderResourceView* GetIrradianceTextureCubeSRV()const { return m_pIrradianceTextureCube->GetShaderResource(); }
	ID3D11ShaderResourceView* GetBRDFLUTTextureSRV()const { return m_pBRDFLUTTexture->GetShaderResource(); }

	ID3D11Texture2D* GetBRDFLUTTexture()const { return m_pBRDFLUTTexture->GetTexture(); }
	GameObject* GetRenderGameObject() { return &m_Cube; }
public:
	

private:
	std::shared_ptr<FirstPersonCamera> m_pCubeCamera;
	GameObject m_Cube;

	float m_CubeMapWidth = 1024.0f;
	float m_CubeMapHeight = 1024.0f;

	DirectX::XMFLOAT3 m_LookForwards[6] = {
		DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(0.0f,  1.0f, 0.0f),
		DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f),
		DirectX::XMFLOAT3(0.0f, 0.0f,  1.0f),
		DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f)
	};
	DirectX::XMFLOAT3 m_LookUps[6] = {
		DirectX::XMFLOAT3(0.0f, 1.0f,  0.0f),
		DirectX::XMFLOAT3(0.0f, 1.0f,  0.0f),
		DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f),
		DirectX::XMFLOAT3(0.0f, 0.0f,  1.0f),
		DirectX::XMFLOAT3(0.0f, 1.0f,  0.0f),
		DirectX::XMFLOAT3(0.0f, 1.0f,  0.0f)
	};

	// TextureCubeの面
	std::vector<std::unique_ptr<Texture2D>> m_pCubeMapSlice;
	// TextureCube
	std::unique_ptr<TextureCube> m_pSkyboxTextureCube;
	int m_MipLevel = 6;
	// IrradianceCubeMap
	std::unique_ptr<TextureCube> m_pIrradianceTextureCube;
	// PrefilterMaps
	std::vector<std::unique_ptr<Texture2D>> m_pPrefilterMaps;
	float m_Roughness = 0.0f;
	// BRDFLUT
	std::unique_ptr<Texture2D> m_pBRDFLUTTexture;

};
#endif