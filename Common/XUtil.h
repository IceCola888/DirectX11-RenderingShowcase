#pragma once

#ifndef XUTIL_H
#define XUTIL_H

#include <DirectXMath.h>
#include <string>
#include <algorithm>
//
// マクロ定義
//

#define LEN_AND_STR(STR) ((UINT)(sizeof(STR) - 1)), (STR)

// グラフィックスデバッグオブジェクト名を有効にするかどうか
#if (defined(DEBUG) || defined(_DEBUG)) && !defined(GRAPHICS_DEBUGGER_OBJECT_NAME)
#define GRAPHICS_DEBUGGER_OBJECT_NAME 1
#endif

//
//  デバッグオブジェクト名を設定する
//

template<class IObject>
inline void SetDebugObjectName(IObject* pObject, std::string_view name)
{
    pObject->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)name.size(), name.data());
}

//
// テキスト変換関数
//

#pragma warning(push)
#pragma warning(disable: 28251)
extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int cp, unsigned long flags, const char* str, int cbmb, wchar_t* widestr, int cchwide);
extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, const wchar_t* widestr, int cchwide, char* str, int cbmb, const char* defchar, int* used_default);
#pragma warning(pop)

inline std::wstring UTF8ToWString(std::string_view utf8str)
{
    if (utf8str.empty()) return std::wstring();
    int cbMultiByte = static_cast<int>(utf8str.size());
    int req = MultiByteToWideChar(65001, 0, utf8str.data(), cbMultiByte, nullptr, 0);
    std::wstring res(req, 0);
    MultiByteToWideChar(65001, 0, utf8str.data(), cbMultiByte, &res[0], req);
    return res;
}

inline std::string WStringToUTF8(std::wstring_view wstr)
{
    if (wstr.empty()) return std::string();
    int cbMultiByte = static_cast<int>(wstr.size());
    int req = WideCharToMultiByte(65001, 0, wstr.data(), cbMultiByte, nullptr, 0, nullptr, nullptr);
    std::string res(req, 0);
    WideCharToMultiByte(65001, 0, wstr.data(), cbMultiByte, &res[0], req, nullptr, nullptr);
    return res;
}

//
// 文字列をhash ID に変換
//

// 型エイリアス定義
// このヘッダーファイルをインクルードしたすべてのソースファイルで、
// XID は size_t の別名として扱われる
using XID = size_t;
inline XID StringToID(std::string_view str)
{
    // 実際には std::string_view 型のデータに対してハッシュ値を計算
    static std::hash<std::string_view> hash;
    // 与えられた文字列ビュー str に対して一意なハッシュ値を返す
    return hash(str);
}

//
// 数学関連の関数
//

namespace XMath
{
    // ------------------------------
    // InverseTranspose関数
    // ------------------------------
    inline DirectX::XMMATRIX XM_CALLCONV InverseTranspose(DirectX::FXMMATRIX M)
    {
        using namespace DirectX;

        // ワールド行列の逆転置は法線ベクトルにのみ適用されるため、
        // ワールド行列の平行移動成分は必要ない。
        // もし削除しない場合、後でビュー行列などを乗算すると
        // 誤った変換結果を引き起こす可能性がある。
        XMMATRIX A = M;
        A.r[3] = g_XMIdentityR3;

        return XMMatrixTranspose(XMMatrixInverse(nullptr, A));
    }

    inline float Lerp(float a, float b, float t)
    {
        return (1.0f - t) * a + t * b;
    }
}

#endif