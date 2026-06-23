#pragma once

#ifndef CAMERA_H
#define CAMERA_H

#include "WinMin.h"
#include <d3d11_1.h>
#include <DirectXMath.h>
#include "Transform.h"

class Camera
{
public:
    Camera() = default;
    virtual ~Camera() = 0;

    //
    // カメラの位置を取得
    //

    DirectX::XMVECTOR GetPositionXM() const;
    DirectX::XMFLOAT3 GetPosition() const;

    //
    // カメラの回転を取得
    //

    // X軸回転のオイラー角（ラジアン）を取得
    float GetRotationX() const;
    // Y軸回転のオイラー角（ラジアン）を取得
    float GetRotationY() const;

    //
    // カメラの座標軸ベクトルを取得
    //

    DirectX::XMVECTOR GetRightAxisXM() const;
    DirectX::XMFLOAT3 GetRightAxis() const;
    DirectX::XMVECTOR GetUpAxisXM() const;
    DirectX::XMFLOAT3 GetUpAxis() const;
    DirectX::XMVECTOR GetLookAxisXM() const;
    DirectX::XMFLOAT3 GetLookAxis() const;

    //
    // 行列を取得
    //

    DirectX::XMMATRIX GetLocalToWorldMatrixXM() const;
    DirectX::XMMATRIX GetViewMatrixXM() const;
    DirectX::XMMATRIX GetProjMatrixXM(bool reversedZ = false) const;
    DirectX::XMMATRIX GetViewProjMatrixXM(bool reversedZ = false) const;

    // ビューポートを取得
    D3D11_VIEWPORT GetViewPort() const;

    float GetNearZ() const;
    float GetFarZ() const;
    float GetFovY() const;
    float GetAspectRatio() const;

    // オイラー角を設定
    void SetRotation(const DirectX::XMFLOAT3& eulerAnglesInRadian);
    void SetRotation(float x, float y, float z);

    // 位置を設定
    void SetPosition(const DirectX::XMFLOAT3& position);
    void SetPosition(float x, float y, float z);

    // 視錐台を設定
    void SetFrustum(float fovY, float aspect, float nearZ, float farZ);

    // ビューポートを設定
    void SetViewPort(const D3D11_VIEWPORT& viewPort);
    void SetViewPort(float topLeftX, float topLeftY, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);

protected:

    // カメラの変換情報
    Transform m_Transform = {};

    // 視錐台のプロパティ
    float m_NearZ = 0.0f;
    float m_FarZ = 0.0f;
    float m_Aspect = 0.0f;
    float m_FovY = 0.0f;

    // 現在のビューポート
    D3D11_VIEWPORT m_ViewPort = {};

};

class FirstPersonCamera : public Camera
{
public:
    FirstPersonCamera() = default;
    ~FirstPersonCamera() override;

    // カメラの位置を設定
    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& pos);
    // カメラの向きを設定
    void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);
    void LookTo(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& to, const DirectX::XMFLOAT3& up);
    // ストレイフ移動（横移動）
    void Strafe(float d);
    // 平面移動
    void Walk(float d);
    // 前進（カメラの前方向へ移動）
    void MoveForward(float d);
    // 指定方向へ移動
    void Translate(const DirectX::XMFLOAT3& dir, float magnitude);
    // 上下の視点移動
    // 正のrad値で上方向に視点移動
    // 負のrad値で下方向に視点移動
    void Pitch(float rad);
    // 左右の視点移動
    // 正のrad値で右方向に視点移動
    // 負のrad値で左方向に視点移動
    void RotateY(float rad);
};

class ThirdPersonCamera : public Camera
{
public:
    ThirdPersonCamera() = default;
    ~ThirdPersonCamera() override;

    // 現在追跡しているオブジェクトの位置を取得
    DirectX::XMFLOAT3 GetTargetPosition() const;
    // オブジェクトとの距離を取得
    float GetDistance() const;
    // オブジェクトの周囲を垂直回転（X軸回転のオイラー角を[0, π/3]の範囲内に制限）
    void RotateX(float rad);
    // オブジェクトの周囲を水平回転
    void RotateY(float rad);
    // オブジェクトに接近
    void Approach(float dist);
    // 初期X軸回転のラジアンを設定（X軸回転のオイラー角を[0, π/3]の範囲内に制限）
    void SetRotationX(float rad);
    // 初期Y軸回転のラジアンを設定
    void SetRotationY(float rad);
    // 追跡対象のオブジェクトの位置を設定
    void SetTarget(const DirectX::XMFLOAT3& target);
    // 初期距離を設定
    void SetDistance(float dist);
    // 許容される最小・最大距離を設定
    void SetDistanceMinMax(float minDist, float maxDist);

private:
    DirectX::XMFLOAT3 m_Target = {};
    float m_Distance = 0.0f;
    // 許容される最小・最大距離
    float m_MinDist = 0.0f, m_MaxDist = 0.0f;
};
#endif
