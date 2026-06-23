#pragma once
#ifndef SSAORENDER_H
#define SSAORENDER_H

#include <d3d11_1.h>
#include <wrl/client.h>
#include <string_view>
#include "Effects.h"
#include <Texture2D.h>
#include <Camera.h>

class SSAOManager 
{
public:
	template<class T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	SSAOManager() = default;
	~SSAOManager() = default;
	// コピー操作は禁止、ムーブ操作のみ許可
	SSAOManager(const SSAOManager&) = delete;
	SSAOManager& operator=(const SSAOManager&) = delete;
	SSAOManager(SSAOManager&&) = default;
	SSAOManager& operator=(SSAOManager&&) = default;

	// リソース初期化
	void InitResource(ID3D11Device* device, int width, int height);
	// リサイズ時のリソース再作成
	void OnResize(ID3D11Device* device, int width, int height);
	// サンプル数設定
	void SetSampleCount(uint32_t count) { m_SampleCount = count; }
	// SSAO生成
	void RenderToSSAOCS(ID3D11DeviceContext* deviceContext, 
		SSAOEffect& ssaoEffect, 
		ID3D11ShaderResourceView* normalGBuffer,
		ID3D11ShaderResourceView* depthGBuffer, 
		const Camera& camera);
	// SSAOブラー
	void BlurCS(ID3D11DeviceContext* deviceContext, SSAOEffect& ssaoEffect);
	// 法線テクスチャ取得
	ID3D11Resource* GetResolveNormalTexture()const { return m_pResolveNormalTexture->GetTexture(); }
	ID3D11ShaderResourceView* GetResolveNormalSRV()const { return m_pResolveNormalTexture->GetShaderResource(); }
	// 深度バッファ（DSVとSRV）取得
	ID3D11DepthStencilView* GetDepthBufferDSV()const { return m_pSSAODepthBuffer->GetDepthStencil(); }
	ID3D11ShaderResourceView* GetDepthBufferSRV()const { return m_pSSAODepthBuffer->GetShaderResource(); }

	/// SSAO最終結果（Ambient Occlusion）テクスチャ取得
	ID3D11ShaderResourceView* GetAmbientOcclusionTexture();

public:
	
	uint32_t m_SampleCount = 14;		// サンプルベクトル数
	uint32_t m_BlurCount = 8;			// ブラー回数
	uint32_t m_BlurRadius = 5;			// ブラー半径

	float m_OcclusionRadius = 0.5f;		// SSAOサンプリング半球の半径
	float m_OcclusionFadeStart = 0.2f;	// フェード開始距離
	float m_OcclusionFadeEnd = 2.0f;	// フェード終了距離
	float m_SurfaceEpsilon = 0.05f;		// 自己遮蔽防止のバイアス
	// ガウスフィルタ用の重み係数（奇数長）
	float m_BlurWeights[11]{
		0.05f, 0.05f,
		0.1f, 0.1f, 0.1f,
		0.2f,
		0.1f, 0.1f, 0.1f,
		0.05f, 0.05f };
	
private:
	// オフセットベクトル生成
	void BuildOffsetVectors();
	// ランダムベクトルテクスチャ生成
	void BuildRandomVectorTexture(ID3D11Device* device);

private:
	DirectX::XMFLOAT4			m_Offsets[14] = {};			// サンプリングオフセット

	std::unique_ptr<Texture2D>  m_pResolveNormalTexture;	// 法線リゾルブ
	std::unique_ptr<Depth2D>	m_pSSAODepthBuffer;			// 深度バッファ
	std::unique_ptr<Texture2D>	m_pAOTexture;				// SSAO最終出力
	std::unique_ptr<Texture2D>	m_pAOTempTexture;			// ブラー中間結果
	std::unique_ptr<Texture2D>	m_pRandomVectorTexture;		// ランダムベクトル
	std::unique_ptr<Texture2D>	m_pAOBlur;					// フィルタ済みAO
};

#endif