#pragma once
#ifndef IEFFECT_H
#define IEFFECT_H

#include "WinMin.h"
#include <memory>
#include <vector>
#include <d3d11_1.h>
#include <DirectXMath.h>

class Material;
struct MeshData;

// 単一の MeshData が入力アセンブリ段階に設定する内容
// 入力レイアウト、ストライド、オフセット、およびプリミティブは Effect Pass によって提供される
// その他のデータは MeshData から取得される
struct MeshDataInput
{
    ID3D11InputLayout* pInputLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    std::vector<ID3D11Buffer*> pVertexBuffers;
    ID3D11Buffer* pIndexBuffer = nullptr;
    std::vector<uint32_t> strides;
    std::vector<uint32_t> offsets;
    uint32_t indexCount = 0;
};

class IEffect
{
public:
    IEffect() = default;
    virtual ~IEffect() = default;
    // コピーは許可されていないが、ムーブは許可されている
    IEffect(const IEffect&) = delete;
    IEffect& operator=(const IEffect&) = delete;
    IEffect(IEffect&&) = default;
    IEffect& operator=(IEffect&&) = default;

    // 定数バッファを更新してバインドする
    virtual void Apply(ID3D11DeviceContext* deviceContext) = 0;
};

class IEffectTransform
{
public:
    virtual void XM_CALLCONV SetWorldMatrix(DirectX::FXMMATRIX W) = 0;
    virtual void XM_CALLCONV SetViewMatrix(DirectX::FXMMATRIX V) = 0;
    virtual void XM_CALLCONV SetProjMatrix(DirectX::FXMMATRIX P) = 0;
};

class IEffectMaterial
{
public:
    virtual void SetMaterial(const Material& material) = 0;
};

class IEffectMeshData
{
public:
    virtual MeshDataInput GetInputData(const MeshData& meshData) = 0;
};


#endif