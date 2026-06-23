#pragma once

#ifndef VERTEX_H
#define VERTEX_H

#include "WinMin.h"
#include <d3d11_1.h>
#include <DirectXMath.h>

// これは配列のコピーではなく、配列の参照である
template<size_t numElements>
using D3D11_INPUT_ELEMENT_DESC_ARRAY = const D3D11_INPUT_ELEMENT_DESC(&)[numElements];


struct VertexPos
{
    VertexPos() = default;

    VertexPos(const VertexPos&) = default;
    VertexPos& operator=(const VertexPos&) = default;

    VertexPos(VertexPos&&) = default;
    VertexPos& operator=(VertexPos&&) = default;

    constexpr VertexPos(const DirectX::XMFLOAT3& _pos) : pos(_pos) {}

    DirectX::XMFLOAT3 pos;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<1> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[1] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct VertexPosColor
{
    VertexPosColor() = default;

    VertexPosColor(const VertexPosColor&) = default;
    VertexPosColor& operator=(const VertexPosColor&) = default;

    VertexPosColor(VertexPosColor&&) = default;
    VertexPosColor& operator=(VertexPosColor&&) = default;

    constexpr VertexPosColor(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT4& _color) :
        pos(_pos), color(_color) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<2> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct VertexPosTex
{
    VertexPosTex() = default;

    VertexPosTex(const VertexPosTex&) = default;
    VertexPosTex& operator=(const VertexPosTex&) = default;

    VertexPosTex(VertexPosTex&&) = default;
    VertexPosTex& operator=(VertexPosTex&&) = default;

    constexpr VertexPosTex(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT2& _tex) :
        pos(_pos), tex(_tex) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<2> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct VertexPosSize
{
    VertexPosSize() = default;

    VertexPosSize(const VertexPosSize&) = default;
    VertexPosSize& operator=(const VertexPosSize&) = default;

    VertexPosSize(VertexPosSize&&) = default;
    VertexPosSize& operator=(VertexPosSize&&) = default;

    constexpr VertexPosSize(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT2& _size) :
        pos(_pos), size(_size) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 size;
    static D3D11_INPUT_ELEMENT_DESC_ARRAY<2> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct VertexPosNormalColor
{
    VertexPosNormalColor() = default;

    VertexPosNormalColor(const VertexPosNormalColor&) = default;
    VertexPosNormalColor& operator=(const VertexPosNormalColor&) = default;

    VertexPosNormalColor(VertexPosNormalColor&&) = default;
    VertexPosNormalColor& operator=(VertexPosNormalColor&&) = default;

    constexpr VertexPosNormalColor(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _normal,
        const DirectX::XMFLOAT4& _color) :
        pos(_pos), normal(_normal), color(_color) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 color;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<3> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[3] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct VertexPosNormalTex
{
    VertexPosNormalTex() = default;

    VertexPosNormalTex(const VertexPosNormalTex&) = default;
    VertexPosNormalTex& operator=(const VertexPosNormalTex&) = default;

    VertexPosNormalTex(VertexPosNormalTex&&) = default;
    VertexPosNormalTex& operator=(VertexPosNormalTex&&) = default;

    constexpr VertexPosNormalTex(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _normal,
        const DirectX::XMFLOAT2& _tex) :
        pos(_pos), normal(_normal), tex(_tex) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 tex;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<3> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[3] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct VertexPosNormalTangentTex
{
    VertexPosNormalTangentTex() = default;

    VertexPosNormalTangentTex(const VertexPosNormalTangentTex&) = default;
    VertexPosNormalTangentTex& operator=(const VertexPosNormalTangentTex&) = default;

    VertexPosNormalTangentTex(VertexPosNormalTangentTex&&) = default;
    VertexPosNormalTangentTex& operator=(VertexPosNormalTangentTex&&) = default;

    constexpr VertexPosNormalTangentTex(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _normal,
        const DirectX::XMFLOAT4& _tangent, const DirectX::XMFLOAT2& _tex) :
        pos(_pos), normal(_normal), tangent(_tangent), tex(_tex) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 tex;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<4> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[4] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        return inputLayout;
    }
};

struct InstVertexPosNormalTex
{
    InstVertexPosNormalTex() = default;

    InstVertexPosNormalTex(const InstVertexPosNormalTex&) = default;
    InstVertexPosNormalTex& operator=(const InstVertexPosNormalTex&) = default;

    InstVertexPosNormalTex(InstVertexPosNormalTex&&) = default;
    InstVertexPosNormalTex& operator=(InstVertexPosNormalTex&&) = default;

    constexpr InstVertexPosNormalTex(const DirectX::XMFLOAT3& _pos, const DirectX::XMFLOAT3& _normal,
        const DirectX::XMFLOAT2& _tex, const DirectX::XMFLOAT4X4& _world, const DirectX::XMFLOAT4X4& _worldInvTrans) :
        pos(_pos), normal(_normal), tex(_tex), world(_world), worldInvTrans(_worldInvTrans) {}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 tex;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 worldInvTrans;

    static D3D11_INPUT_ELEMENT_DESC_ARRAY<11> GetInputLayout()
    {
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[11] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,1,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,2,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"World",0,DXGI_FORMAT_R32G32B32A32_FLOAT,3,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"World",1,DXGI_FORMAT_R32G32B32A32_FLOAT,3,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"World",2,DXGI_FORMAT_R32G32B32A32_FLOAT,3,32,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"World",3,DXGI_FORMAT_R32G32B32A32_FLOAT,3,48,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WorldInvTranspose",0,DXGI_FORMAT_R32G32B32A32_FLOAT,3,64,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WorldInvTranspose",1,DXGI_FORMAT_R32G32B32A32_FLOAT,3,80,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WorldInvTranspose",2,DXGI_FORMAT_R32G32B32A32_FLOAT,3,96,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WorldInvTranspose",3,DXGI_FORMAT_R32G32B32A32_FLOAT,3,112,D3D11_INPUT_PER_INSTANCE_DATA,1}
        };
        return inputLayout;
    }
};

#endif