#include "Collision.h"
#include "GameObject.h"
using namespace DirectX;

Ray::Ray()
	: origin(), direction(0.0f, 0.0f, 1.0f)
{
}

Ray::Ray(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction)
	: origin(origin)
{
	XMVECTOR dirVec = XMLoadFloat3(&direction);
	assert(XMVector3NotEqual(dirVec, g_XMZero));
	XMStoreFloat3(&this->direction, XMVector3Normalize(dirVec));
}

Ray Ray::ScreenToRay(const Camera& camera, float screenX, float screenY)
{
	// ******************
	// DirectX::XMVector3Unproject関数からの抜粋し、
	// ワールド座標系からローカル座標系への変換を省略
	//

	//Vndc = Vscreen * scale + offset
	//Vworld = Vndc * PInv * VInv

	// 画面座標をNDCに変換


	// 最外側の中括弧：XMVECTORF32 型のインスタンス全体を初期化
	// 二番目の中括弧：XMVECTORF32 内部の匿名共用体を対応
	// 最内側の中括弧：float[4] 配列 f を直接初期化
	static const XMVECTORF32 D = { { { -1.0f, 1.0f, 0.0f, 0.0f } } };
	// マウスクリック位置のベクトル
	XMVECTOR V = XMVectorSet(screenX, screenY, 0.0f, 1.0f);
	D3D11_VIEWPORT viewPort = camera.GetViewPort();

	XMVECTOR Scale = XMVectorSet(viewPort.Width * 0.5f, -viewPort.Height * 0.5f, viewPort.MaxDepth - viewPort.MinDepth, 1.0f);
	Scale = XMVectorReciprocal(Scale);

	XMVECTOR Offset = XMVectorSet(-viewPort.TopLeftX, -viewPort.TopLeftY, -viewPort.MinDepth, 0.0f);
	Offset = XMVectorMultiplyAdd(Scale, Offset, D.v);

	// NDC座標系からワールド座標系へ変換
	XMMATRIX Transform = XMMatrixMultiply(camera.GetViewMatrixXM(), camera.GetProjMatrixXM());
	Transform = XMMatrixInverse(nullptr, Transform);

	XMVECTOR Target = XMVectorMultiplyAdd(V, Scale, Offset);
	Target = XMVector3TransformCoord(Target, Transform);

	// レイを計算
	XMFLOAT3 direction;
	XMStoreFloat3(&direction, Target - camera.GetPositionXM());
	return Ray(camera.GetPosition(), direction);
}

bool Ray::Hit(const DirectX::BoundingBox& box, float* pOutDist, float maxDist)
{

	float dist;
	bool res = box.Intersects(XMLoadFloat3(&origin), XMLoadFloat3(&direction), dist);
	if (pOutDist)
		*pOutDist = dist;
	return dist > maxDist ? false : res;
}

bool Ray::Hit(const DirectX::BoundingOrientedBox& box, float* pOutDist, float maxDist)
{
	float dist;
	bool res = box.Intersects(XMLoadFloat3(&origin), XMLoadFloat3(&direction), dist);
	if (pOutDist)
		*pOutDist = dist;
	return dist > maxDist ? false : res;
}

bool Ray::Hit(const DirectX::BoundingSphere& sphere, float* pOutDist, float maxDist)
{
	float dist;
	bool res = sphere.Intersects(XMLoadFloat3(&origin), XMLoadFloat3(&direction), dist);
	if (pOutDist)
		*pOutDist = dist;
	return dist > maxDist ? false : res;
}

bool XM_CALLCONV Ray::Hit(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2, float* pOutDist, float maxDist)
{
	float dist;
	bool res = TriangleTests::Intersects(XMLoadFloat3(&origin), XMLoadFloat3(&direction), V0, V1, V2, dist);
	if (pOutDist)
		*pOutDist = dist;
	return dist > maxDist ? false : res;
}

Collision::WireFrameData Collision::CreateBoundingBox(ID3D11Device* device, const DirectX::BoundingBox& box, const DirectX::XMFLOAT4& color)
{
	XMFLOAT3 corners[8];
	box.GetCorners(corners);
	return CreateFromCorners(device, corners, color);
}

Collision::WireFrameData Collision::CreateBoundingOrientedBox(ID3D11Device* device, const DirectX::BoundingOrientedBox& box, const DirectX::XMFLOAT4& color)
{
	XMFLOAT3 corners[8];
	box.GetCorners(corners);
	return CreateFromCorners(device, corners, color);
}

Collision::WireFrameData Collision::CreateBoundingSphere(ID3D11Device* device, const DirectX::BoundingSphere& sphere, const DirectX::XMFLOAT4& color, int slices)
{
	WireFrameData data;
	XMVECTOR center = XMLoadFloat3(&sphere.Center), posVec;
	XMFLOAT3 pos;
	float theta = 0.0f;
	for (int i = 0; i < slices; ++i)
	{
		posVec = XMVector3Transform(center + XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), XMMatrixRotationY(theta));
		XMStoreFloat3(&pos, posVec);
		data.vertexVec.push_back({ pos, color });
		posVec = XMVector3Transform(center + XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), XMMatrixRotationZ(theta));
		XMStoreFloat3(&pos, posVec);
		data.vertexVec.push_back({ pos, color });
		posVec = XMVector3Transform(center + XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f), XMMatrixRotationX(theta));
		XMStoreFloat3(&pos, posVec);
		data.vertexVec.push_back({ pos, color });
		theta += XM_2PI / slices;
	}
	for (int i = 0; i < slices; ++i)
	{
		data.indexVec.push_back(i * 3);
		data.indexVec.push_back((i + 1) % slices * 3);

		data.indexVec.push_back(i * 3 + 1);
		data.indexVec.push_back((i + 1) % slices * 3 + 1);

		data.indexVec.push_back(i * 3 + 2);
		data.indexVec.push_back((i + 1) % slices * 3 + 2);
	}


	return data;
}

Collision::WireFrameData Collision::CreateBoundingFrustum(ID3D11Device* device, const DirectX::BoundingFrustum& frustum, const DirectX::XMFLOAT4& color)
{
	XMFLOAT3 corners[8];
	frustum.GetCorners(corners);
	return CreateFromCorners(device, corners, color);
}

std::vector<Transform> XM_CALLCONV Collision::FrustumCulling(
	const std::vector<Transform>& transforms, const DirectX::BoundingBox& localBox, DirectX::FXMMATRIX View, DirectX::CXMMATRIX Proj)
{
	std::vector<Transform> acceptedData;

	BoundingFrustum frustum;
	BoundingFrustum::CreateFromMatrix(frustum, Proj);

	BoundingOrientedBox localOrientedBox, orientedBox;
	BoundingOrientedBox::CreateFromBoundingBox(localOrientedBox, localBox);
	for (auto& t : transforms)
	{
		XMMATRIX W = t.GetLocalToWorldMatrixXM();
		// 有向バウンディングボックスをローカル座標系から視錐台のローカル座標系（ビュー座標系）へ変換
		localOrientedBox.Transform(orientedBox, W * View);
		// 衝突判定
		if (frustum.Intersects(orientedBox))
			acceptedData.push_back(t);
	}

	return acceptedData;
}

void XM_CALLCONV Collision::FrustumCulling(
	std::vector<Transform>& dest, const std::vector<Transform>& src, const DirectX::BoundingBox& localBox, DirectX::FXMMATRIX View, DirectX::CXMMATRIX Proj)
{
	dest.clear();

	BoundingFrustum frustum;
	BoundingFrustum::CreateFromMatrix(frustum, Proj);

	BoundingOrientedBox localOrientedBox, orientedBox;
	BoundingOrientedBox::CreateFromBoundingBox(localOrientedBox, localBox);
	for (auto& t : src)
	{
		XMMATRIX W = t.GetLocalToWorldMatrixXM();
		// 有向バウンディングボックスをローカル座標系から視錐台のローカル座標系（ビュー座標系）へ変換
		localOrientedBox.Transform(orientedBox, W * View);
		// 衝突判定
		if (frustum.Intersects(orientedBox))
			dest.push_back(t);
	}
}

Collision::WireFrameData Collision::CreateFromCorners(ID3D11Device* device, const DirectX::XMFLOAT3(&corners)[8], const DirectX::XMFLOAT4& color)
{
	WireFrameData data;
	// AABB/OBB の頂点インデックス   視錐台の頂点インデックス
	//     3_______2                     4__________5
	//    /|      /|                     |\        /|
	//  7/_|____6/ |                     | \      / |
	//  |  |____|__|                    7|_0\____/1_|6
	//  | /0    | /1                      \ |    | /
	//  |/______|/                         \|____|/
	//  4       5                           3     2
	for (int i = 0; i < 8; ++i)
		data.vertexVec.push_back({ corners[i], color });
	for (int i = 0; i < 4; ++i)
	{
		data.indexVec.push_back(i);
		data.indexVec.push_back(i + 4);

		data.indexVec.push_back(i);
		data.indexVec.push_back((i + 1) % 4);

		data.indexVec.push_back(i + 4);
		data.indexVec.push_back((i + 1) % 4 + 4);
	}
	return data;
}