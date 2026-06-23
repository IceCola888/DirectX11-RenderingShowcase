#pragma once

#ifndef PROPERTY_H
#define PROPERTY_H

#include <memory>
#include <variant>
#include <vector>
#include <string>
#include <DirectXMath.h>

//----------------------------------------------------------------
// コンパイル時に、ある型 `T` が `std::variant` 内に
// 定義された型のいずれかであるかをチェックするための仕組み
//

// この行は、テンプレート構造体の具体的な実装を定義するのではなく、
// そのようなテンプレート構造体が存在することを宣言しているだけ
template<class T, class V>
struct IsVariantMember;

// 特化（特殊化）。第二テンプレートパラメータが std::variant 型の場合に適用される
template<class T, class... ALL_V>
struct IsVariantMember<T, std::variant<ALL_V...>> :
    // std::disjunction はテンプレートであり、テンプレートパラメータパックを受け取る
    // パラメータパック内に少なくとも1つ true の値を持つ要素があれば、std::disjunction の値は true になる
    public std::disjunction<std::is_same<T, ALL_V>...> {};

// union と同様に、1つの変数に異なる型の値を格納できる
// しかし `std::variant` は、より安全かつ柔軟に異なる型のデータを扱うことができる
using Property = std::variant<
    int, uint32_t, float, DirectX::XMFLOAT2, DirectX::XMFLOAT3, DirectX::XMFLOAT4, DirectX::XMFLOAT4X4,
    std::vector<float>, std::vector<DirectX::XMFLOAT4>, std::vector<DirectX::XMFLOAT4X4>,
    std::string>;

#endif
