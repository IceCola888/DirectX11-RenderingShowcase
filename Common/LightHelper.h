#ifndef LIGHTHELPER_H
#define LIGHTHELPER_H

#include <cstring>
#include <DirectXMath.h>


// ディレクショナルライト
struct DirectionalLight
{
    DirectionalLight() = default;

    DirectionalLight(const DirectionalLight&) = default;
    DirectionalLight& operator=(const DirectionalLight&) = default;

    DirectionalLight(DirectionalLight&&) = default;
    DirectionalLight& operator=(DirectionalLight&&) = default;

    DirectionalLight(const DirectX::XMFLOAT3& _color, float _intensity, const DirectX::XMFLOAT3& _direction,
        float _pad1) :
        color(_color), intensity(_intensity), direction(_direction), pad1() {}

    DirectX::XMFLOAT3 color;
    float intensity;

    DirectX::XMFLOAT3 direction;
    float pad1; // 最後に浮動小数点数で埋めて、この構造体のサイズが16の倍数になるように調整し、 HLSLで配列を設定しやすくする
};

// ポイントライト
struct PointLight
{
    PointLight() = default;

    PointLight(const PointLight&) = default;
    PointLight& operator=(const PointLight&) = default;

    PointLight(PointLight&&) = default;
    PointLight& operator=(PointLight&&) = default;

    PointLight(const DirectX::XMFLOAT3& _position, float _range, const DirectX::XMFLOAT3& _color,
        float _intensity) :
        position(_position), range(_range), color(_color), intensity(_intensity) {}

    DirectX::XMFLOAT3 position;
    float range;

    DirectX::XMFLOAT3 color;
    float intensity;
};

// スポットライト
// spotAngles.x(inner costheta) < spotAngles.y(outer costheta) の必要がある
struct SpotLight
{
    SpotLight() = default;

    SpotLight(const SpotLight&) = default;
    SpotLight& operator=(const SpotLight&) = default;

    SpotLight(SpotLight&&) = default;
    SpotLight& operator=(SpotLight&&) = default;

    SpotLight(const DirectX::XMFLOAT3& _color, float _intensity, const DirectX::XMFLOAT3& _position,
        float _range, const DirectX::XMFLOAT3& _direction,
        float _spotRadius, const DirectX::XMFLOAT2& _spotAngles) :
        color(_color), intensity(_intensity), position(_position),
        range(_range), direction(_direction), spotRadius(_spotRadius), spotAngles(_spotAngles), pad2() {}

    DirectX::XMFLOAT3 color;
    float intensity;

    DirectX::XMFLOAT3 position;
    float range;

    DirectX::XMFLOAT3 direction;
    float spotRadius;

    // spotAngles.x for inner costheta  
    // spotAngles.y for outer costheta
    DirectX::XMFLOAT2 spotAngles;
    DirectX::XMFLOAT2 pad2;
};

#endif
