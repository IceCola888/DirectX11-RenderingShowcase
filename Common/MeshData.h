#pragma once

#ifndef MESH_DATA_H
#define MESH_DATA_H

#include <wrl/client.h>
#include <vector>
#include <DirectXCollision.h>
#include <Vertex.h>
struct ID3D11Buffer;

// a polygon mesh is 
// a collection of vertices, edges and faces 
// that defines the shape of a polyhedral object

struct MeshData
{
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    ComPtr<ID3D11Buffer> m_pVertices;
    ComPtr<ID3D11Buffer> m_pNormals;

    std::vector<ComPtr<ID3D11Buffer>> m_pTexcoordArrays;
    ComPtr<ID3D11Buffer> m_pTangents;
    ComPtr<ID3D11Buffer> m_pBitangents;
    ComPtr<ID3D11Buffer> m_pColors;

    ComPtr<ID3D11Buffer> m_pIndices;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
    uint32_t m_MaterialIndex = 0;

    DirectX::BoundingBox m_BoundingBox;
    ComPtr<ID3D11Buffer> m_pBBVB;
    ComPtr<ID3D11Buffer> m_pBBIB;
    ComPtr<ID3D11Buffer> m_pBBColors;
    uint32_t m_BBVertexCount = 0;
    uint32_t m_BBIndexCount = 0;

    DirectX::BoundingSphere m_BoundingSphere;
    ComPtr<ID3D11Buffer> m_pBSVB;
    ComPtr<ID3D11Buffer> m_pBSIB;
    ComPtr<ID3D11Buffer> m_pBSColors;
    uint32_t m_BSVertexCount = 0;
    uint32_t m_BSIndexCount = 0;

    bool m_InFrustum = true;
};

#endif