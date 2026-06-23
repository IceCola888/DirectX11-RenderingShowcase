#pragma once

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <DirectXMath.h>

class Transform
{
public:
    Transform() = default;
    Transform(const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT3& position)
        : m_Scale(scale), m_Position(position)
    {
        SetRotation(rotation);
    }
    ~Transform() = default;

    Transform(const Transform&) = default;
    Transform& operator=(const Transform&) = default;

    Transform(Transform&&) = default;
    Transform& operator=(Transform&&) = default;

    // オブジェクトのスケール値を取得
    DirectX::XMFLOAT3 GetScale() const { return m_Scale; }
    // オブジェクトのスケール値を取得
    DirectX::XMVECTOR GetScaleXM() const { return XMLoadFloat3(&m_Scale); }

    // オブジェクトのオイラー角を取得（ラジアン単位）
    // オブジェクトはZ-X-Y軸の順で回転
    DirectX::XMFLOAT3 GetRotation() const
    {
        float sinX = 2 * (m_Rotation.w * m_Rotation.x - m_Rotation.y * m_Rotation.z);
        float sinY_cosX = 2 * (m_Rotation.w * m_Rotation.y + m_Rotation.x * m_Rotation.z);
        float cosY_cosX = 1 - 2 * (m_Rotation.x * m_Rotation.x + m_Rotation.y * m_Rotation.y);
        float sinZ_cosX = 2 * (m_Rotation.w * m_Rotation.z + m_Rotation.x * m_Rotation.y);
        float cosZ_cosX = 1 - 2 * (m_Rotation.x * m_Rotation.x + m_Rotation.z * m_Rotation.z);

        DirectX::XMFLOAT3 rotation;
        // 特殊ケース（ジンバルロック）を処理
        if (fabs(fabs(sinX) - 1.0f) < 1e-5f)
        {
            rotation.x = copysignf(DirectX::XM_PI / 2, sinX);
            rotation.y = 2.0f * atan2f(m_Rotation.y, m_Rotation.w);
            rotation.z = 0.0f;
        }
        else
        {
            rotation.x = asinf(sinX);
            rotation.y = atan2f(sinY_cosX, cosY_cosX);
            rotation.z = atan2f(sinZ_cosX, cosZ_cosX);
        }

        return rotation;
    }

    // オブジェクトの回転クォータニオンを取得
    DirectX::XMFLOAT4 GetRotationQuat() const { return m_Rotation; }
    // オブジェクトのオイラー角を取得（ラジアン単位）
    // オブジェクトはZ-X-Y軸の順で回転
    DirectX::XMVECTOR GetRotationXM() const { auto rot = GetRotation(); return XMLoadFloat3(&rot); }
    // オブジェクトの回転クォータニオンを取得
    DirectX::XMVECTOR GetRotationQuatXM() const { return XMLoadFloat4(&m_Rotation); }

    // オブジェクトの位置を取得
    DirectX::XMFLOAT3 GetPosition() const { return m_Position; }
    // オブジェクトの位置を取得
    DirectX::XMVECTOR GetPositionXM() const { return XMLoadFloat3(&m_Position); }

    // 右方向の軸を取得
    DirectX::XMFLOAT3 GetRightAxis() const
    {
        using namespace DirectX;
        XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&m_Rotation));
        XMFLOAT3 right;
        XMStoreFloat3(&right, R.r[0]);
        return right;
    }
    // 右方向の軸を取得
    DirectX::XMVECTOR GetRightAxisXM() const
    {
        DirectX::XMFLOAT3 right = GetRightAxis();
        return DirectX::XMLoadFloat3(&right);
    }

    // 上方向の軸を取得
    DirectX::XMFLOAT3 GetUpAxis() const
    {
        using namespace DirectX;
        XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&m_Rotation));
        XMFLOAT3 up;
        XMStoreFloat3(&up, R.r[1]);
        return up;
    }
    // 上方向の軸を取得
    DirectX::XMVECTOR GetUpAxisXM() const
    {
        DirectX::XMFLOAT3 up = GetUpAxis();
        return DirectX::XMLoadFloat3(&up);
    }

    // 前方向の軸を取得
    DirectX::XMFLOAT3 GetForwardAxis() const
    {
        using namespace DirectX;
        XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&m_Rotation));
        XMFLOAT3 forward;
        XMStoreFloat3(&forward, R.r[2]);
        return forward;
    }
    // 前方向の軸を取得
    DirectX::XMVECTOR GetForwardAxisXM() const
    {
        DirectX::XMFLOAT3 forward = GetForwardAxis();
        return DirectX::XMLoadFloat3(&forward);
    }

    // ワールド変換行列を取得
    DirectX::XMFLOAT4X4 GetLocalToWorldMatrix() const
    {
        DirectX::XMFLOAT4X4 res;
        DirectX::XMStoreFloat4x4(&res, GetLocalToWorldMatrixXM());
        return res;
    }
    // ワールド変換行列を取得
    DirectX::XMMATRIX GetLocalToWorldMatrixXM() const
    {
        using namespace DirectX;
        DirectX::XMVECTOR scaleVec = XMLoadFloat3(&m_Scale);
        DirectX::XMVECTOR quateration = XMLoadFloat4(&m_Rotation);
        DirectX::XMVECTOR positionVec = XMLoadFloat3(&m_Position);
        DirectX::XMMATRIX World = XMMatrixAffineTransformation(scaleVec, g_XMZero, quateration, positionVec);
        return World;
    }

    // 逆ワールド変換行列を取得
    DirectX::XMFLOAT4X4 GetWorldToLocalMatrix() const
    {
        DirectX::XMFLOAT4X4 res;
        DirectX::XMStoreFloat4x4(&res, GetWorldToLocalMatrixXM());
        return res;
    }
    // 逆ワールド変換行列を取得
    DirectX::XMMATRIX GetWorldToLocalMatrixXM() const
    {
        DirectX::XMMATRIX InvWorld = DirectX::XMMatrixInverse(nullptr, GetLocalToWorldMatrixXM());
        return InvWorld;
    }

    // オブジェクトのスケールを設定
    void SetScale(const DirectX::XMFLOAT3& scale) { m_Scale = scale; }
    // オブジェクトのスケールを設定
    void SetScale(float x, float y, float z) { m_Scale = DirectX::XMFLOAT3(x, y, z); }

    // オブジェクトのオイラー角を設定（ラジアン単位）
    // オブジェクトはZ-X-Y軸の順で回転
    void SetRotation(const DirectX::XMFLOAT3& eulerAnglesInRadian)
    {
        auto quat = DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&eulerAnglesInRadian));
        DirectX::XMStoreFloat4(&m_Rotation, quat);
    }
    // オブジェクトのオイラー角を設定（ラジアン単位）
    // オブジェクトはZ-X-Y軸の順で回転
    void SetRotation(float x, float y, float z)
    {
        auto quat = DirectX::XMQuaternionRotationRollPitchYaw(x, y, z);
        DirectX::XMStoreFloat4(&m_Rotation, quat);
    }

    // オブジェクトの位置を設定
    void SetPosition(const DirectX::XMFLOAT3& position) { m_Position = position; }
    // オブジェクトの位置を設定
    void SetPosition(float x, float y, float z) { m_Position = DirectX::XMFLOAT3(x, y, z); }

    // 指定したオイラー角でオブジェクトを回転
    void Rotate(const DirectX::XMFLOAT3& eulerAnglesInRadian)
    {
        using namespace DirectX;
        auto newQuat = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&eulerAnglesInRadian));
        auto quat = XMLoadFloat4(&m_Rotation);
        XMStoreFloat4(&m_Rotation, XMQuaternionMultiply(quat, newQuat));
    }
    // 指定した軸を中心に回転
    void RotateAxis(const DirectX::XMFLOAT3& axis, float radian)
    {
        using namespace DirectX;
        auto newQuat = XMQuaternionRotationAxis(XMLoadFloat3(&axis), radian);
        auto quat = XMLoadFloat4(&m_Rotation);
        XMStoreFloat4(&m_Rotation, XMQuaternionMultiply(quat, newQuat));
    }

    // 指定した点を中心に軸回転
    void RotateAround(const DirectX::XMFLOAT3& point, const DirectX::XMFLOAT3& axis, float radian)
    {
        using namespace DirectX;
        XMVECTOR quat = XMLoadFloat4(&m_Rotation);
        XMVECTOR positionVec = XMLoadFloat3(&m_Position);
        XMVECTOR centerVec = XMLoadFloat3(&point);

        // pointを原点として
        XMMATRIX RT = XMMatrixRotationQuaternion(quat) * XMMatrixTranslationFromVector(positionVec - centerVec);
        RT *= XMMatrixRotationAxis(XMLoadFloat3(&axis), radian);
        RT *= XMMatrixTranslationFromVector(centerVec);
        XMStoreFloat4(&m_Rotation, XMQuaternionRotationMatrix(RT));
        XMStoreFloat3(&m_Position, RT.r[3]);
    }
    // 指定した方向に移動
    void Translate(const DirectX::XMFLOAT3& direction, float magnitude)
    {
        using namespace DirectX;
        XMVECTOR directionVec = XMVector3Normalize(XMLoadFloat3(&direction));
        XMVECTOR newPosition = XMVectorMultiplyAdd(XMVectorReplicate(magnitude), directionVec, XMLoadFloat3(&m_Position));
        XMStoreFloat3(&m_Position, newPosition);
    }

    // 指定した点を見る
    void LookAt(const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up = { 0.0f, 1.0f, 0.0f })
    {
        using namespace DirectX;
        XMMATRIX View = XMMatrixLookAtLH(XMLoadFloat3(&m_Position), XMLoadFloat3(&target), XMLoadFloat3(&up));
        XMMATRIX InvView = XMMatrixInverse(nullptr, View);
        XMStoreFloat4(&m_Rotation, XMQuaternionRotationMatrix(InvView));
    }

    // 指定した方向を向く
    void LookTo(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& up = { 0.0f, 1.0f, 0.0f })
    {
        using namespace DirectX;
        XMMATRIX View = XMMatrixLookToLH(XMLoadFloat3(&m_Position), XMLoadFloat3(&direction), XMLoadFloat3(&up));
        XMMATRIX InvView = XMMatrixInverse(nullptr, View);
        XMStoreFloat4(&m_Rotation, XMQuaternionRotationMatrix(InvView));
    }

    // 回転行列からオイラー角を取得
    static DirectX::XMFLOAT3 GetEulerAnglesFromRotationMatrix(const DirectX::XMFLOAT4X4& rotationMatrix)
    {
        DirectX::XMFLOAT3 rotation{};
        // 特殊ケース（ジンバルロック）の処理
        if (fabs(1.0f - fabs(rotationMatrix(2, 1))) < 1e-5f)
        {
            rotation.x = copysignf(DirectX::XM_PIDIV2, -rotationMatrix(2, 1));
            rotation.y = -atan2f(rotationMatrix(0, 2), rotationMatrix(0, 0));
            return rotation;
        }

        // 回転行列からオイラー角を計算
        float c = sqrtf(1.0f - rotationMatrix(2, 1) * rotationMatrix(2, 1));
        // r[2][1] の値が1を超えることを防ぐ
        if (isnan(c))
            c = 0.0f;

        rotation.z = atan2f(rotationMatrix(0, 1), rotationMatrix(1, 1));
        rotation.x = atan2f(-rotationMatrix(2, 1), c);
        rotation.y = atan2f(rotationMatrix(2, 0), rotationMatrix(2, 2));
        return rotation;
    }
private:
    DirectX::XMFLOAT3 m_Scale = { 1.0f, 1.0f, 1.0f };				// スケール
    DirectX::XMFLOAT4 m_Rotation = { 0.0f, 0.0f, 0.0f, 1.0f };		// 回転（クォータニオン）
    DirectX::XMFLOAT3 m_Position = {};								// 位置
};
#endif