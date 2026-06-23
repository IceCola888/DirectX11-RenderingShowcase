#pragma once

#ifndef MATERIAL_H
#define MATERIAL_H

#include <string_view>
#include <unordered_map>
#include "XUtil.h"
#include "Property.h"

class Material
{
public:
    Material() = default;

    void Clear()
    {
        m_Properties.clear();
    }

    // name は例えばマテリアル情報の名前 (例: diffuse)
    // value はその一意に対応する値であり、Property の一種 (例: XMFLOAT3)
    template<class T>
    void Set(std::string_view name, const T& value)
    {
        // 提供された型 T が Property に受け入れられる型の一つかどうかをチェックする
        static_assert(IsVariantMember<T, Property>::value, "Type T isn't one of the Property types!");
        // operator[] は配列ではなく、unordered_map の要素にアクセスするために使用される
        // name 文字列を XID 型に変換する
        // その後、m_Properties という std::unordered_map に
        // この XID をキーとして対応する value を格納または更新する
        m_Properties[StringToID(name)] = value;
    }

    template<class T>
    const T& Get(std::string_view name) const
    {
        auto it = m_Properties.find(StringToID(name));
        return std::get<T>(it->second);
    }

    template<class T>
    T& Get(std::string_view name)
    {
         // 定数参照を非定数参照に変換する
        return const_cast<T&>(
            // 定数 Material オブジェクトへのポインタに変換し
            // その後、定数版の Get を呼び出す
            static_cast<const Material*>(this)->Get<T>(name));
    }

    template<class T>
    bool Has(std::string_view name) const
    {
        return TryGet<T>(name) != nullptr;
    }

    // 実行時に型の不一致によるクラッシュを防ぐことを保証する。
    template<class T>
    const T* TryGet(std::string_view name) const
    {
        auto it = m_Properties.find(StringToID(name));
        if (it != m_Properties.end() && std::holds_alternative<T>(it->second))
            return &std::get<T>(it->second);
        else
            return nullptr;
    }

    template<class T>
    T* TryGet(std::string_view name)
    {
        return const_cast<T*>(static_cast<const Material*>(this)->TryGet<T>(name));
    }

    // XID を使用して対応する property が存在するかを検索する。
    bool HasProperty(std::string_view name) const
    {
        return m_Properties.find(StringToID(name)) != m_Properties.end();
    }

    void Remove(std::string_view name)
    {
        m_Properties.erase(StringToID(name));
    }

private:
    // 連想コンテナ
    // std::map と似ているが、内部の実装と性能特性が異なる。

    // unordered_map は要素を順序なしで格納する。
    // そのため、要素の順序に依存することはできない。
    // これは std::map とは異なり、std::map はキー値に基づいて要素をソートして保持する。

    // 平均的な場合、検索、挿入、および削除操作の時間計算量は定数時間（O(1)）。
    //  しかし、最悪の場合、これらの操作の時間計算量は O(n) に退化する可能性がある。
    // ここで n はコンテナ内の要素数である。

    // XID を使用して Property のキーと値のペア変数を識別する。
    std::unordered_map<XID, Property> m_Properties;
};

#endif