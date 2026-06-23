#pragma once

#ifndef COLLISION_H
#define COLLISION_H

#include <DirectXCollision.h>
#include <vector>
#include "Vertex.h"
#include "Camera.h"
#include <wrl/client.h>

struct Ray
{
	Ray();
	Ray(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction);

	static Ray ScreenToRay(const Camera& camera, float screenX, float screenY);

	bool Hit(const DirectX::BoundingBox& box, float* pOutDist = nullptr, float maxDist = FLT_MAX);
	bool Hit(const DirectX::BoundingOrientedBox& box, float* pOutDist = nullptr, float maxDist = FLT_MAX);
	bool Hit(const DirectX::BoundingSphere& sphere, float* pOutDist = nullptr, float maxDist = FLT_MAX);
	bool XM_CALLCONV Hit(DirectX::FXMVECTOR V0, DirectX::FXMVECTOR V1, DirectX::FXMVECTOR V2, float* pOutDist = nullptr, float maxDist = FLT_MAX);

	DirectX::XMFLOAT3 origin;		// レイの原点
	DirectX::XMFLOAT3 direction;	// 単位方向ベクトル
};

class Collision
{
public:

	// ワイヤーフレームの頂点/インデックス配列
	struct WireFrameData
	{
		std::vector<VertexPosColor> vertexVec;		// 頂点配列
		std::vector<uint32_t> indexVec;				// インデックス配列
	};


	//
	//　バウンディングボックスワイヤーフレームの作成
	//

	// AABBボックスのワイヤーフレームを作成
	static WireFrameData CreateBoundingBox(ID3D11Device* device, const DirectX::BoundingBox& box, const DirectX::XMFLOAT4& color);
	// OBBボックスのワイヤーフレームを作成
	static WireFrameData CreateBoundingOrientedBox(ID3D11Device* device, const DirectX::BoundingOrientedBox& box, const DirectX::XMFLOAT4& color);
	// バウンディングスフィアのワイヤーフレームを作成
	static WireFrameData CreateBoundingSphere(ID3D11Device* device, const DirectX::BoundingSphere& sphere, const DirectX::XMFLOAT4& color, int slices = 20);
	// 視錐台のワイヤーフレームを作成
	static WireFrameData CreateBoundingFrustum(ID3D11Device* device, const DirectX::BoundingFrustum& frustum, const DirectX::XMFLOAT4& color);

	// 視錐台クリッピング
	static std::vector<Transform> XM_CALLCONV FrustumCulling(
		const std::vector<Transform>& transforms, const DirectX::BoundingBox& localBox, DirectX::FXMMATRIX View, DirectX::CXMMATRIX Proj);

	// 視錐台クリッピング
	static void XM_CALLCONV FrustumCulling(
		std::vector<Transform>& dest, const std::vector<Transform>& src,
		const DirectX::BoundingBox& localBox, DirectX::FXMMATRIX View, DirectX::CXMMATRIX Proj);

private:
	static WireFrameData CreateFromCorners(ID3D11Device* device, const DirectX::XMFLOAT3(&corners)[8], const DirectX::XMFLOAT4& color);
};


#endif