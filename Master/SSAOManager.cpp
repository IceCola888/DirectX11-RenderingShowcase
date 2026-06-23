#include "SSAOManager.h"
#include <DirectXPackedVector.h>
#include <XUtil.h>
#include <random>

#pragma warning(disable: 26812)

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

void SSAOManager::InitResource(ID3D11Device* device, int width, int height)
{
	OnResize(device, width, height);
	BuildOffsetVectors();
	BuildRandomVectorTexture(device);
}

void SSAOManager::OnResize(ID3D11Device* device, int width, int height)
{
	m_pResolveNormalTexture = std::make_unique<Texture2D>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_pSSAODepthBuffer = std::make_unique<Depth2D>(device, width, height, 
		DepthStencilBitsFlag::Depth_32Bits);

	m_pAOTempTexture = std::make_unique<Texture2D>(device, width / 2, height / 2, DXGI_FORMAT_R16_FLOAT, 1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_pAOTexture = std::make_unique<Texture2D>(device, width / 2, height / 2, DXGI_FORMAT_R16_FLOAT, 1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
	m_pAOBlur = std::make_unique<Texture2D>(device, width / 2, height / 2, DXGI_FORMAT_R16_FLOAT, 1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);

	m_pResolveNormalTexture->SetDebugObjectName("ResolveNormalTexture");
	m_pSSAODepthBuffer->SetDebugObjectName("NormalDepthTexture");
	m_pAOTexture->SetDebugObjectName("SSAOTexture");
	m_pAOTempTexture->SetDebugObjectName("SSAOTempTexture");
	m_pAOBlur->SetDebugObjectName("SSAOBlur");
}

void SSAOManager::BuildOffsetVectors()
{
	// 均等に分布した14個のベクトルから開始する
	// 立方体の8つの頂点を選び、各面の中央からもベクトルを取る
	// これらの点は常に対向する位置から交互に出現するように配置される
	// この方法により、14個未満のサンプル数でもベクトルが均一に分散される

	// 立方体の8つの頂点方向のベクトル
	m_Offsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	m_Offsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	m_Offsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	m_Offsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	m_Offsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	m_Offsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	m_Offsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	m_Offsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 立方体の6つの面の中心方向のベクトル
	m_Offsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	m_Offsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	m_Offsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	m_Offsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	m_Offsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	m_Offsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);


	// ランダムベクトルマップの初期化
	std::mt19937 randEngine;
	randEngine.seed(std::random_device()());
	std::uniform_real_distribution<float> randF(0.25f, 1.0f);
	for (int i = 0; i < 14; ++i)
	{
		// [0.25, 1.0] の範囲でランダムな長さを持つベクトルを生成する
		float s = randF(randEngine);

		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&m_Offsets[i]));
		XMStoreFloat4(&m_Offsets[i], v);
	}
}

void SSAOManager::BuildRandomVectorTexture(ID3D11Device* device)
{
	m_pRandomVectorTexture = std::make_unique<Texture2D>(device, 256, 256, DXGI_FORMAT_R8G8B8A8_UNORM);
	m_pRandomVectorTexture->SetDebugObjectName("RandomVectorTexture");

	std::vector<XMCOLOR> randomVectors(256 * 256);

	ComPtr<ID3D11DeviceContext> pContext;
	device->GetImmediateContext(pContext.GetAddressOf());

	// ランダムベクトルマップの初期化
	std::mt19937 randEngine;
	randEngine.seed(std::random_device()());
	std::uniform_real_distribution<float> randF(0.0f, 1.0f);
	for (int i = 0; i < 256 * 256; ++i)
		randomVectors[i] = XMCOLOR(randF(randEngine), randF(randEngine), randF(randEngine), 0.0f);

	pContext->UpdateSubresource(m_pRandomVectorTexture->GetTexture(), 0, nullptr, randomVectors.data(), 256 * sizeof(XMCOLOR), 0);

}


void SSAOManager::RenderToSSAOCS(ID3D11DeviceContext* deviceContext,
	SSAOEffect& ssaoEffect, 
	ID3D11ShaderResourceView* normalGBuffer,
	ID3D11ShaderResourceView* depthGBuffer, 
	const Camera& camera)
{
	float zFar = camera.GetFarZ();
	float halfHeight = zFar * tanf(0.5f * camera.GetFovY());
	float halfWidth = camera.GetAspectRatio() * halfHeight;


	XMFLOAT4 farSquarePoints[4] = {
		XMFLOAT4(-halfWidth, halfHeight, zFar, 0.0f),
		XMFLOAT4(halfWidth, halfHeight, zFar, 0.0f),
		XMFLOAT4(-halfWidth,-halfHeight, zFar, 0.0f),
		XMFLOAT4(halfWidth,-halfHeight, zFar, 0.0f),
	};
	ssaoEffect.SetFrustumFarPlaneSquardPoints(farSquarePoints);

	ssaoEffect.SetTextureRandomVec(m_pRandomVectorTexture->GetShaderResource());
	ssaoEffect.SetOffsetVectors(m_Offsets);
	ssaoEffect.SetProjMatrix(camera.GetProjMatrixXM(true));
	ssaoEffect.SetOcclusionInfo(m_OcclusionRadius, m_OcclusionFadeStart, m_OcclusionFadeEnd, m_SurfaceEpsilon);

	CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)m_pAOTexture->GetWidth(), (float)m_pAOTexture->GetHeight());
	ssaoEffect.SetSSAOTextureSize(m_pAOTexture->GetWidth(), m_pAOTexture->GetHeight());
	ssaoEffect.RenderToSSAOCS(deviceContext,
		normalGBuffer, depthGBuffer,
		m_pAOTexture->GetUnorderedAccess(), vp, m_SampleCount);
}

void SSAOManager::BlurCS(ID3D11DeviceContext* deviceContext, SSAOEffect& ssaoEffect)
{

	ssaoEffect.SetBlurRadius(m_BlurRadius);
	ssaoEffect.SetBlurWeights(m_BlurWeights);
	// Blur処理は毎回、水平方向と垂直方向の2回実行する必要がある
	for (uint32_t i = 0; i < m_BlurCount; ++i)
	{
		ssaoEffect.ComputeBlurX(deviceContext, m_pAOTexture->GetShaderResource(),
			m_pResolveNormalTexture->GetShaderResource(),
			m_pSSAODepthBuffer->GetShaderResource(),
			m_pAOBlur->GetUnorderedAccess(), m_pAOBlur->GetWidth(), m_pAOBlur->GetHeight());
		ssaoEffect.ComputeBlurY(deviceContext, m_pAOBlur->GetShaderResource(), 
			m_pResolveNormalTexture->GetShaderResource(),
			m_pSSAODepthBuffer->GetShaderResource(),
			m_pAOTexture->GetUnorderedAccess(), m_pAOBlur->GetWidth(), m_pAOBlur->GetHeight());
	}
}

ID3D11ShaderResourceView* SSAOManager::GetAmbientOcclusionTexture()
{
	return m_pAOTexture->GetShaderResource();
}
