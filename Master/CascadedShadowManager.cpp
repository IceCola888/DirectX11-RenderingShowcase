#include "CascadedShadowManager.h"

using namespace DirectX;

HRESULT CascadedShadowManager::InitResource(ID3D11Device* device)
{
    DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT;
    
    switch (m_ShadowType)
    {
    case ShadowType::ShadowType_ESM:
    case ShadowType::ShadowType_CSM: format = DXGI_FORMAT_R32_FLOAT; break;
    case ShadowType::ShadowType_VSM:
    case ShadowType::ShadowType_EVSM2: format = DXGI_FORMAT_R32G32_FLOAT; break;
    case ShadowType::ShadowType_EVSM4: format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
    }
   
    m_pCSMTextureArray = std::make_unique<Texture2DArray>(device, m_ShadowSize, m_ShadowSize, format,
        (uint32_t)m_CascadeLevels, 1);
    m_pCSMTempTexture = std::make_unique<Texture2D>(device, m_ShadowSize, m_ShadowSize, format, 1);

    m_pCSMDepthBuffer = std::make_unique<Depth2D>(device, m_ShadowSize, m_ShadowSize, DepthStencilBitsFlag::Depth_32Bits);

    m_ShadowViewport.TopLeftX = 0;
    m_ShadowViewport.TopLeftY = 0;
    m_ShadowViewport.Width = (float)m_ShadowSize;
    m_ShadowViewport.Height = (float)m_ShadowSize;
    m_ShadowViewport.MinDepth = 0.0f;
    m_ShadowViewport.MaxDepth = 1.0f;

    m_pCSMTempTexture->SetDebugObjectName("CSM Temp Texture");
    m_pCSMTextureArray->SetDebugObjectName("CSM Texture Array");
    m_pCSMDepthBuffer->SetDebugObjectName("CSM Depth Buffer");

    return S_OK;
}

void CascadedShadowManager::UpdateFrame(
    const Camera& viewerCamera, const Camera& lightCamera,
    const DirectX::BoundingBox& sceneBoundingBox)
{
    // PlayerCameraから投影行列を取得
    XMMATRIX ViewerProj = viewerCamera.GetProjMatrixXM();
    // PlayerCameraからビュー行列を取得
    XMMATRIX ViewerView = viewerCamera.GetViewMatrixXM();
    // LightCameraからビュー行列を取得
    XMMATRIX LightView = lightCamera.GetViewMatrixXM();
    // LightCameraのビュー行列の逆行列
    XMMATRIX ViewerInvView = XMMatrixInverse(nullptr, ViewerView);

    float frustumIntervalBegin, frustumIntervalEnd;

    // 光空間における視錐台のAABBの最小点 vMin
    XMVECTOR lightCameraOrthographicMinVec;
    // 光空間における視錐台のAABBの最大点 vMax
    XMVECTOR lightCameraOrthographicMaxVec;
    // PlayerCameraの深度範囲
    float cameraNearFarRange = viewerCamera.GetFarZ() - viewerCamera.GetNearZ();



    //
    // 各カスケードに対して光空間での正射影行列を計算
    //
    for (int cascadeIndex = 0; cascadeIndex < m_CascadeLevels; ++cascadeIndex)
    {
        // 各カスケードの視錐台の開始深度を決定
        // 現在のCascadeがカバーする視錐台範囲をZ軸の最小/最大距離で評価して計算
        if (m_SelectedCascadesFit == FitProjection::FitProjection_ToCascade)
        {
            // 正射影行列がカスケードにぴったり合うように
            // 前のカスケードの終端を次の開始点として使用
            if (cascadeIndex == 0)
                frustumIntervalBegin = 0.0f;
            else
                frustumIntervalBegin = m_CascadePartitionsPercentage[cascadeIndex - 1];
        }
        else
        {
            // FitProjection_ToSceneモードでは、カスケード間でオーバーラップが発生する
            // 例えばカスケード1〜8は範囲1をカバーし
            // 2〜8は範囲2をカバーする
            frustumIntervalBegin = 0.0f;
        }

        
        // 比率によって各CascadeのdepthMaxを計算
        frustumIntervalEnd = m_CascadePartitionsPercentage[cascadeIndex] * cameraNearFarRange;
        // 比率によって各CascadeのdepthMinを計算
        frustumIntervalBegin = frustumIntervalBegin * cameraNearFarRange;


        // プレイヤーカメラの視錐台を表すBoundingFrustumを作成（ViewSpace）
        XMFLOAT3 viewerFrustumPoints[8];
        BoundingFrustum viewerFrustum(ViewerProj);
        // Near/Far Planeとして保存
        viewerFrustum.Near = frustumIntervalBegin;
        viewerFrustum.Far = frustumIntervalEnd;

        // カメラ空間での視錐台をまずWorldSpaceへ、その後LightSpaceへ変換
        viewerFrustum.Transform(viewerFrustum, ViewerInvView * LightView);
        // LightSpaceにおける視錐台の頂点を取得
        viewerFrustum.GetCorners(viewerFrustumPoints);

        // LightSpaceでの視錐台のAABBを計算
        BoundingBox viewerFrustumBox;
        BoundingBox::CreateFromPoints(viewerFrustumBox, 8, viewerFrustumPoints, sizeof(XMFLOAT3));
        // AABBのローカル座標における最大/最小ベクトル
        lightCameraOrthographicMaxVec = XMLoadFloat3(&viewerFrustumBox.Center) + XMLoadFloat3(&viewerFrustumBox.Extents);
        lightCameraOrthographicMinVec = XMLoadFloat3(&viewerFrustumBox.Center) - XMLoadFloat3(&viewerFrustumBox.Extents);

        if (m_FixedSizeFrustumAABB)
        {
            // カスケード0のニア・ファー距離が短すぎると、斜めの対角線がファー面より短くなる
            // その結果、AABBが視錐台全体をカバーできない可能性がある
            // 子視錐台の斜対角線とファー面の対角線のうち、長い方をXYサイズとして使用し、AABBを固定サイズにする
            // サイズが十分に大きいため、常にカスケードをカバーできる

            //     Near    Far
            //    0----1  4----5
            //    |    |  |    |
            //    |    |  |    |
            //    3----2  7----6

            // 子視錐台の斜対角線
            XMVECTOR diagVec = XMLoadFloat3(viewerFrustumPoints + 7) - XMLoadFloat3(viewerFrustumPoints + 1);
            // FarPlaneの対角線
            XMVECTOR diag2Vec = XMLoadFloat3(viewerFrustumPoints + 7) - XMLoadFloat3(viewerFrustumPoints + 5);
            // より長い対角線をAABBのXYサイズとする
            XMVECTOR lengthVec = XMVectorMax(XMVector3Length(diagVec), XMVector3Length(diag2Vec));

            // 差分をAABBの最大点・最小点に等しく加減して、中心位置を維持する
            XMVECTOR borderOffsetVec = (lengthVec - (lightCameraOrthographicMaxVec - lightCameraOrthographicMinVec)) * g_XMOneHalf.v;
            // XY方向のみを拡張対象とする
            static const XMVECTORF32 xyzw1100Vec = { {1.0f,1.0f,0.0f,0.0f} };
            // 最大点・最小点を外側に拡張
            lightCameraOrthographicMaxVec += borderOffsetVec * xyzw1100Vec.v;
            lightCameraOrthographicMinVec -= borderOffsetVec * xyzw1100Vec.v;
        }

        // PCFカーネルサイズに基づいてAABBを追加で拡張
        {
            float scaleDuetoBlur = m_PCFKernelSize / (float)m_ShadowSize;
            XMVECTORF32 scaleDuetoBlurVec = { {scaleDuetoBlur, scaleDuetoBlur, 0.0f, 0.0f} };

            XMVECTOR borderOffsetVec = lightCameraOrthographicMaxVec - lightCameraOrthographicMinVec;
            borderOffsetVec *= g_XMOneHalf.v;
            borderOffsetVec *= scaleDuetoBlurVec.v;
            lightCameraOrthographicMaxVec += borderOffsetVec;
            lightCameraOrthographicMinVec -= borderOffsetVec;
        }

        if (m_MoveLightTexelSize)
        {
            XMVECTOR worldUnitsPerTexelVec = g_XMZero;

            // シャドウマップ内の各テクセルが持つワールド単位の大きさを計算し、シャドウのチラつきを防止
            float normalizeByBufferSize = 1.0f / m_ShadowSize;

            worldUnitsPerTexelVec = lightCameraOrthographicMaxVec - lightCameraOrthographicMinVec;
            worldUnitsPerTexelVec *= normalizeByBufferSize;

            // worldUnitsPerTexel
            // | |                     LightSpace
            // [x][x][ ]    [ ][x][x]  xはShadowTexel
            // [x][x][ ] => [ ][x][x]
            // [ ][ ][ ]    [ ][ ][ ]
            // カメラが動いても、AABBはすぐには動かず、一定の距離で移動する
            // テクセルサイズ分の変化が蓄積されたときにAABBが更新され、ステップ的に動く
            // そのため、カメラを動かしてもシャドウがチラつかない
            lightCameraOrthographicMinVec /= worldUnitsPerTexelVec;
            // eg. -2.3 -> -3 
            lightCameraOrthographicMinVec = XMVectorFloor(lightCameraOrthographicMinVec);
            lightCameraOrthographicMinVec *= worldUnitsPerTexelVec;

            lightCameraOrthographicMaxVec /= worldUnitsPerTexelVec;
            lightCameraOrthographicMaxVec = XMVectorFloor(lightCameraOrthographicMaxVec);
            lightCameraOrthographicMaxVec *= worldUnitsPerTexelVec;
        }

        float nearPlane = 0.0f;
        float farPlane = 0.0f;

        // シーン内のオブジェクトのAABBをLightSpaceに変換
        XMVECTOR sceneAABBPointsLightSpace[8]{};
        {
            XMFLOAT3 corners[8];
            sceneBoundingBox.GetCorners(corners);
            for (int i = 0; i < 8; ++i)
            {
                XMVECTOR v = XMLoadFloat3(corners + i);
                sceneAABBPointsLightSpace[i] = XMVector3Transform(v, LightView);
            }
        }

        //
        // 各カスケードにおける光源視点のNera/Far Planeの選び方
        //
        if (m_SelectedNearFarFit == FitNearFar::FitNearFar_ZeroOne)
        {
            // 以下の値は、正確なニア・ファーの設定の重要性を示すための例（設定ミスでひどい結果になる）
            nearPlane = 0.1f;
            farPlane = 10000.0f;
        }
        if (m_SelectedNearFarFit == FitNearFar::FitNearFar_CascadeAABB)
        {
            // 視錐台AABBの外の情報が無いため、ニア平面の外にあるオブジェクトはシャドウマップに投影されない
            // 結果として影が正しく生成されない
            nearPlane = XMVectorGetZ(lightCameraOrthographicMinVec);
            farPlane = XMVectorGetZ(lightCameraOrthographicMaxVec);
        }
        else if (m_SelectedNearFarFit == FitNearFar::FitNearFar_SceneAABB)
        {
            XMVECTOR lightSpaceSceneAABBminValueVec = g_XMFltMax.v;
            XMVECTOR lightSpaceSceneAABBmaxValueVec = -g_XMFltMax.v;
            // LightSpaceにおけるシーンの最小/最大ベクトルを計算
            // LightSpaceAABBのminZ/maxZをNear/Farに使用
            // シーンAABBとの交差判定より簡単で、状況によっては同等の結果が得られる
            for (int i = 0; i < 8; ++i)
            {
                lightSpaceSceneAABBminValueVec = XMVectorMin(sceneAABBPointsLightSpace[i], lightSpaceSceneAABBminValueVec);
                lightSpaceSceneAABBmaxValueVec = XMVectorMax(sceneAABBPointsLightSpace[i], lightSpaceSceneAABBmaxValueVec);
            }
            nearPlane = XMVectorGetZ(lightSpaceSceneAABBminValueVec);
            farPlane = XMVectorGetZ(lightSpaceSceneAABBmaxValueVec);
        }
        else if (m_SelectedNearFarFit == FitNearFar::FitNearFar_SceneAABB_Intersection)
        {
            // LightSpaceでの視錐台AABBとシーンAABBの交差判定により、よりタイトなNear/Far Planeを算出可能
            ComputeNearAndFar(nearPlane, farPlane, lightCameraOrthographicMinVec, lightCameraOrthographicMaxVec,
                sceneAABBPointsLightSpace);
        }

        // 各カスケードに対する光空間での正射影行列を生成
        XMStoreFloat4x4(m_ShadowProj + cascadeIndex,
            XMMatrixOrthographicOffCenterLH(
                XMVectorGetX(lightCameraOrthographicMinVec), XMVectorGetX(lightCameraOrthographicMaxVec),
                XMVectorGetY(lightCameraOrthographicMinVec), XMVectorGetY(lightCameraOrthographicMaxVec),
                nearPlane, farPlane));

        // 最終的な正射影用のAABBを作成
        lightCameraOrthographicMinVec = XMVectorSetZ(lightCameraOrthographicMinVec, nearPlane);
        lightCameraOrthographicMaxVec = XMVectorSetZ(lightCameraOrthographicMaxVec, farPlane);
        BoundingBox::CreateFromPoints(m_ShadowProjBoundingBox[cascadeIndex],
            lightCameraOrthographicMinVec, lightCameraOrthographicMaxVec);

        // 各カスケードの最大深度値を記録し、後続処理に使用
        m_CascadePartitionsFrustum[cascadeIndex] = frustumIntervalEnd;
    }


}

struct Triangle
{
    XMVECTOR point[3];
    bool isCulled;
};

//--------------------------------------------------------------------------------------
// 正確なニア・ファー平面を計算することで、Surface AcneやPeter-panningを軽減できる
// 通常はPCFフィルタリングでバイアスを加えてシャドウアーチファクトを軽減するが、
// この方法では投影行列の精度自体を改善できる
//--------------------------------------------------------------------------------------
void XM_CALLCONV CascadedShadowManager::ComputeNearAndFar(
    float& outNearPlane,
    float& outFarPlane,
    FXMVECTOR lightCameraOrthographicMinVec,
    FXMVECTOR lightCameraOrthographicMaxVec,
    XMVECTOR pointsInCameraView[])
{
    // コア：
    // 1. AABBを構成する全12三角形を処理する
    // 2. 各三角形に対して、正射影ボリュームの4つの側面に対してクリッピングを行う
    //    - 頂点が全て外側：除去
    //    - 1頂点が内側：2つの交点を補間し新三角形に
    //    - 2頂点が内側：2つの三角形に分割
    //    - 全頂点が内側：そのまま
    //    クリップ後の三角形も同様に残りの平面で再度クリッピング
    // 3. 最終的に残った三角形群のZ値の最小/最大をニア・ファー平面とする

    outNearPlane = FLT_MAX;
    outFarPlane = -FLT_MAX;
    // 最大16個の三角形が生成される可能性
    Triangle triangleList[16]{};
    // 三角形リストの数
    int numTriangles;

    //      4----5
    //     /|   /| 
    //    0-+--1 | 
    //    | 7--|-6
    //    |/   |/  
    //    3----2

    
    static const int all_indices[][3] = {
        {4,7,6}, {6,5,4},
        {5,6,2}, {2,1,5},
        {1,2,3}, {3,0,1},
        {0,3,7}, {7,4,0},
        {7,3,2}, {2,6,7},
        {0,4,5}, {5,1,0}
    };
    bool triPointPassCollision[3]{};

    // 左
    const float minX = XMVectorGetX(lightCameraOrthographicMinVec);
    // 右
    const float maxX = XMVectorGetX(lightCameraOrthographicMaxVec);
    // 下
    const float minY = XMVectorGetY(lightCameraOrthographicMinVec);
    // 上
    const float maxY = XMVectorGetY(lightCameraOrthographicMaxVec);


    for (auto& indices : all_indices)
    {
        // 初期三角形セットアップ（AABBの各面）
        triangleList[0].point[0] = pointsInCameraView[indices[0]];
        triangleList[0].point[1] = pointsInCameraView[indices[1]];
        triangleList[0].point[2] = pointsInCameraView[indices[2]];
        numTriangles = 1;
        triangleList[0].isCulled = false;

        // 各三角形に対して4つの側面でのクリッピング処理
        for (int planeIdx = 0; planeIdx < 4; ++planeIdx)
        {
            float edge;
            // x軸->0   Y軸->1
            int component;
            switch (planeIdx)
            {
            case 0: edge = minX; component = 0; break;
            case 1: edge = maxX; component = 0; break;
            case 2: edge = minY; component = 1; break;
            case 3: edge = maxY; component = 1; break;
            default: break;
            }

            // 三角形リスト内の各三角形に対してクリッピング テストを実行します
            // (三角形は後で複数の三角形にクリッピングされる場合があります)
            for (int triIdx = 0; triIdx < numTriangles; ++triIdx)
            {
                // 切り取られた三角形をスキップする
                if (triangleList[triIdx].isCulled)
                    continue;

                int insideVertexCount = 0;

                // 現在の三角形の各頂点の位置を決定します
                for (int triVtxIdx = 0; triVtxIdx < 3; ++triVtxIdx)
                {
                    switch (planeIdx)
                    {
                    case 0: triPointPassCollision[triVtxIdx] = (XMVectorGetX(triangleList[triIdx].point[triVtxIdx]) > minX); break;
                    case 1: triPointPassCollision[triVtxIdx] = (XMVectorGetX(triangleList[triIdx].point[triVtxIdx]) < maxX); break;
                    case 2: triPointPassCollision[triVtxIdx] = (XMVectorGetY(triangleList[triIdx].point[triVtxIdx]) > minY); break;
                    case 3: triPointPassCollision[triVtxIdx] = (XMVectorGetY(triangleList[triIdx].point[triVtxIdx]) < maxY); break;
                    default: break;
                    }
                    // クリッピング面内の頂点の数を数える
                    insideVertexCount += triPointPassCollision[triVtxIdx];
                }

                // 内部頂点を先頭にソート
                if (triPointPassCollision[1] && !triPointPassCollision[0])
                {
                    std::swap(triangleList[triIdx].point[0], triangleList[triIdx].point[1]);
                    triPointPassCollision[0] = true;
                    triPointPassCollision[1] = false;
                }
                if (triPointPassCollision[2] && !triPointPassCollision[1])
                {
                    std::swap(triangleList[triIdx].point[1], triangleList[triIdx].point[2]);
                    triPointPassCollision[1] = true;
                    triPointPassCollision[2] = false;
                }
                if (triPointPassCollision[1] && !triPointPassCollision[0])
                {
                    std::swap(triangleList[triIdx].point[0], triangleList[triIdx].point[1]);
                    triPointPassCollision[0] = true;
                    triPointPassCollision[1] = false;
                }

                // 全頂点が外側の場合カリング
                triangleList[triIdx].isCulled = (insideVertexCount == 0);

                // 頂点数に応じた分岐
                if (insideVertexCount == 1)
                {
                    // 補間して新三角形に変換
                    XMVECTOR v0v1Vec = triangleList[triIdx].point[1] - triangleList[triIdx].point[0];
                    XMVECTOR v0v2Vec = triangleList[triIdx].point[2] - triangleList[triIdx].point[0];

                    // 裁断面内にある1点から、他の2点が裁断される方向への距離を取得
                    float hitPointRatio = edge - XMVectorGetByIndex(triangleList[triIdx].point[0], component);
                    // 距離を対応する辺の長さで割り、補間用の係数とする
                    float distAlong_v0v1 = hitPointRatio / XMVectorGetByIndex(v0v1Vec, component);
                    float distAlong_v0v2 = hitPointRatio / XMVectorGetByIndex(v0v2Vec, component);
                    // 比率に従ってトリミングされた辺の長さを計算します
                    v0v1Vec = distAlong_v0v1 * v0v1Vec + triangleList[triIdx].point[0];
                    v0v2Vec = distAlong_v0v2 * v0v2Vec + triangleList[triIdx].point[0];

                    triangleList[triIdx].point[1] = v0v2Vec;
                    triangleList[triIdx].point[2] = v0v1Vec;
                }
                else if (insideVertexCount == 2)
                {
                    // 裁断によって三角形を2つに分割する

                    // 現在の三角形の後ろにあるデータをコピーして
                    // 後で計算された三角形で上書きできるようにする
                    triangleList[numTriangles] = triangleList[triIdx + 1];
                    triangleList[triIdx + 1].isCulled = false;

                    // 裁断平面と交差する2つの辺を計算
                    XMVECTOR v2v0Vec = triangleList[triIdx].point[0] - triangleList[triIdx].point[2];
                    XMVECTOR v2v1Vec = triangleList[triIdx].point[1] - triangleList[triIdx].point[2];

                    float hitPointRatio = edge - XMVectorGetByIndex(triangleList[triIdx].point[2], component);
                    float distAlong_v2v0 = hitPointRatio / XMVectorGetByIndex(v2v0Vec, component);
                    float distAlong_v2v1 = hitPointRatio / XMVectorGetByIndex(v2v1Vec, component);
                    v2v0Vec = distAlong_v2v0 * v2v0Vec + triangleList[triIdx].point[2];
                    v2v1Vec = distAlong_v2v1 * v2v1Vec + triangleList[triIdx].point[2];

                    // 新しい三角形を1つ追加
                    triangleList[triIdx + 1].point[0] = triangleList[triIdx].point[0];
                    triangleList[triIdx + 1].point[1] = triangleList[triIdx].point[1];
                    triangleList[triIdx + 1].point[2] = v2v0Vec;

                    triangleList[triIdx].point[0] = triangleList[triIdx + 1].point[1];
                    triangleList[triIdx].point[1] = triangleList[triIdx + 1].point[2];
                    triangleList[triIdx].point[2] = v2v1Vec;

                    // 三角形数を加算し、挿入済みの三角形をスキップ
                    ++numTriangles;
                    ++triIdx;
                }
            }
        }

        // 最終的に残った三角形群のZ範囲を集計
        for (int triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            if (!triangleList[triIdx].isCulled)
            {
                for (int vtxIdx = 0; vtxIdx < 3; ++vtxIdx)
                {
                    float z = XMVectorGetZ(triangleList[triIdx].point[vtxIdx]);

                    outNearPlane = (std::min)(outNearPlane, z);
                    outFarPlane = (std::max)(outFarPlane, z);
                }
            }
        }
    }
}