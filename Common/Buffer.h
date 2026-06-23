#pragma once

#ifndef BUFFER_H
#define BUFFER_H

#include "WinMin.h"
#include "D3DFormat.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <string_view>

class Buffer
{
public:
    Buffer(ID3D11Device* d3dDevice, const CD3D11_BUFFER_DESC& bufferDesc);
    Buffer(ID3D11Device* d3dDevice, D3D11_SUBRESOURCE_DATA* pData,
        const CD3D11_BUFFER_DESC& bufferDesc);
    Buffer(ID3D11Device* d3dDevice, const CD3D11_BUFFER_DESC& bufferDesc,
        const CD3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const CD3D11_UNORDERED_ACCESS_VIEW_DESC& uavDesc);
    Buffer(ID3D11Device* d3dDevice, D3D11_SUBRESOURCE_DATA* pData,
        const CD3D11_BUFFER_DESC& bufferDesc,
        const CD3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc,
        const CD3D11_UNORDERED_ACCESS_VIEW_DESC& uavDesc);
    ~Buffer() = default;

    // コピー禁止、ムーブのみ許可
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = default;
    Buffer& operator=(Buffer&&) = default;

    ID3D11Buffer* GetBuffer() { return m_pBuffer.Get(); }
    ID3D11Buffer** GetBufferAddress() { return m_pBuffer.GetAddressOf(); }
    ID3D11UnorderedAccessView* GetUnorderedAccess() { return m_pUnorderedAccess.Get(); }
    ID3D11ShaderResourceView* GetShaderResource() { return m_pShaderResource.Get(); }

    // 動的バッファのみサポート
    void* MapDiscard(ID3D11DeviceContext* d3dDeviceContext);
    void Unmap(ID3D11DeviceContext* d3dDeviceContext);
    uint32_t GetByteWidth() const { return m_ByteWidth; }

    // デバッグオブジェクト名を設定
    void SetDebugObjectName(std::string_view name);

protected:
    template<class Type>
    using ComPtr = Microsoft::WRL::ComPtr<Type>;

    ComPtr<ID3D11Buffer> m_pBuffer;
    ComPtr<ID3D11ShaderResourceView> m_pShaderResource;
    ComPtr<ID3D11UnorderedAccessView> m_pUnorderedAccess;
    uint32_t m_ByteWidth = 0;
};

//
// 間接パラメータバッファ IndirectArgumentsBuffer
//
class IndirectArgsBuffer : public Buffer
{
public:
    IndirectArgsBuffer(ID3D11Device* d3dDevice,
        D3D11_SUBRESOURCE_DATA* pData,
        UINT byteWidth, uint32_t bindFlags = 0);
    ~IndirectArgsBuffer() = default;

    // コピー禁止、ムーブのみ許可
    IndirectArgsBuffer(const IndirectArgsBuffer&) = delete;
    IndirectArgsBuffer& operator=(const IndirectArgsBuffer&) = delete;
    IndirectArgsBuffer(IndirectArgsBuffer&&) = default;
    IndirectArgsBuffer& operator=(IndirectArgsBuffer&&) = default;

};

inline IndirectArgsBuffer::IndirectArgsBuffer(ID3D11Device* d3dDevice,
    D3D11_SUBRESOURCE_DATA* pData,
    UINT byteWidth, uint32_t bindFlags)
    :Buffer(d3dDevice, pData,
        CD3D11_BUFFER_DESC(byteWidth, bindFlags, D3D11_USAGE_DEFAULT, 0, D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS, 0))
{

}


// 注意: Tとシェーダー内の構造体のサイズ/レイアウトが一致していることを確認してください
template<class T>
class StructuredBuffer : public Buffer
{
public:
    StructuredBuffer(ID3D11Device* d3dDevice, uint32_t elements,
        uint32_t bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
        bool enableCounter = false,
        bool dynamic = false,
        bool enableDrawDirect = false);
    ~StructuredBuffer() = default;

    // コピー禁止、ムーブのみ許可
    StructuredBuffer(const StructuredBuffer&) = delete;
    StructuredBuffer& operator=(const StructuredBuffer&) = delete;
    StructuredBuffer(StructuredBuffer&&) = default;
    StructuredBuffer& operator=(StructuredBuffer&&) = default;

    // 動的バッファのみサポート
    T* MapDiscard(ID3D11DeviceContext* d3dDeviceContext);

    uint32_t GetNumElements() const { return m_Elements; }


private:
    uint32_t m_Elements;
};

template<class T>
inline StructuredBuffer<T>::StructuredBuffer(ID3D11Device* d3dDevice, uint32_t elements, uint32_t bindFlags, bool enableCounter, bool dynamic, bool enableDrawDirect)
    : m_Elements(elements),
    Buffer(d3dDevice,
        CD3D11_BUFFER_DESC(sizeof(T)* elements, bindFlags,
            dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
            dynamic ? D3D11_CPU_ACCESS_WRITE : 0,
            enableDrawDirect ? D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS : D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            sizeof(T)),
        CD3D11_SHADER_RESOURCE_VIEW_DESC(enableDrawDirect ? D3D11_SRV_DIMENSION_BUFFEREX : D3D11_SRV_DIMENSION_BUFFER, enableDrawDirect ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_UNKNOWN, 0, elements),
        CD3D11_UNORDERED_ACCESS_VIEW_DESC(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, 0, elements, 0, D3D11_BUFFER_UAV_FLAG_COUNTER))
{
}

template <typename T>
T* StructuredBuffer<T>::MapDiscard(ID3D11DeviceContext* d3dDeviceContext)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3dDeviceContext->Map(m_pBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    return static_cast<T*>(mappedResource.pData);
}



template<class T>
class AppendConsumeBuffer :public Buffer
{
public:
    AppendConsumeBuffer(ID3D11Device* device, D3D11_SUBRESOURCE_DATA* pData,
        uint32_t elements,
        uint32_t bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
    ~AppendConsumeBuffer() = default;

    // コピー禁止、ムーブのみ許可
    AppendConsumeBuffer(const AppendConsumeBuffer&) = delete;
    AppendConsumeBuffer& operator=(const AppendConsumeBuffer&) = delete;
    AppendConsumeBuffer(AppendConsumeBuffer&&) = default;
    AppendConsumeBuffer& operator=(AppendConsumeBuffer&&) = default;

    uint32_t GetNumElements()const { return m_Elements; }

private:
    uint32_t m_Elements;
};

template<class T>
inline AppendConsumeBuffer<T>::AppendConsumeBuffer(ID3D11Device* d3dDevice, D3D11_SUBRESOURCE_DATA* pData, uint32_t elements, uint32_t bindFlags)
    :m_Elements(elements),
    Buffer(d3dDevice, pData,
        CD3D11_BUFFER_DESC(sizeof(T)* elements, bindFlags,
            D3D11_USAGE_DEFAULT,
            0,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            sizeof(T)),
        CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, 0, elements),
        CD3D11_UNORDERED_ACCESS_VIEW_DESC(D3D11_UAV_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN, 0, elements, -1, D3D11_BUFFER_UAV_FLAG_APPEND))
{
}


template <DXGI_FORMAT format>
struct TypedBuffer : public Buffer
{
public:
    TypedBuffer(ID3D11Device* d3dDevice, uint32_t numElems,
        uint32_t bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
        bool dynamic = false);
    ~TypedBuffer() = default;

    // コピー禁止、ムーブのみ許可
    TypedBuffer(const TypedBuffer&) = delete;
    TypedBuffer& operator=(const TypedBuffer&) = delete;
    TypedBuffer(TypedBuffer&&) = default;
    TypedBuffer& operator=(TypedBuffer&&) = default;

    uint32_t GetNumElements() const { return m_Elements; }

private:
    uint32_t m_Elements;
};

template<DXGI_FORMAT format>
TypedBuffer<format>::TypedBuffer(ID3D11Device* d3dDevice, uint32_t numElems, uint32_t bindFlags, bool dynamic)
    : m_Elements(numElems), Buffer(d3dDevice, CD3D11_BUFFER_DESC(
        GetFormatSize(format)* numElems, bindFlags,
        dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
        dynamic ? D3D11_CPU_ACCESS_WRITE : 0),
        CD3D11_SHADER_RESOURCE_VIEW_DESC(m_pBuffer.Get(), format, 0, numElems),
        CD3D11_UNORDERED_ACCESS_VIEW_DESC(m_pBuffer.Get(), format, 0, numElems))
{
}

struct ByteAddressBuffer : public Buffer
{
public:
    ByteAddressBuffer(ID3D11Device* d3dDevice, uint32_t numUInt32s,
        uint32_t bindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
        bool dynamic = false, bool indirectArgs = false);
    ~ByteAddressBuffer() = default;

    // コピー禁止、ムーブのみ許可
    ByteAddressBuffer(const ByteAddressBuffer&) = delete;
    ByteAddressBuffer& operator=(const ByteAddressBuffer&) = delete;
    ByteAddressBuffer(ByteAddressBuffer&&) = default;
    ByteAddressBuffer& operator=(ByteAddressBuffer&&) = default;

    uint32_t GetNumUInt32s() const { return m_NumUInt32s; }

private:
    uint32_t m_NumUInt32s;
};
#endif