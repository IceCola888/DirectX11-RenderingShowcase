#pragma once

#ifndef GPU_TIMER_H
#define GPU_TIMER_H

#include <cstdint>
#include <cassert>
#include <deque>
#include <wrl/client.h>
#include "WinMin.h"
#include <d3d11_1.h>

struct GpuTimerInfo
{
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData{}; // 周波数/連続性情報
    uint64_t startData = 0;                             // 開始タイムスタンプ
    uint64_t stopData = 0;                              // 終了タイムスタンプ
    Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;  // 連続性クエリ
    Microsoft::WRL::ComPtr<ID3D11Query> startQuery;     // 開始タイムスタンプクエリ
    Microsoft::WRL::ComPtr<ID3D11Query> stopQuery;      // 終了タイムスタンプクエリ
    bool isStopped = false;                             // 終了タイムスタンプが挿入されたかどうか
};

class GpuTimer
{
public:
    GpuTimer() = default;

    // recentCount が 0 の場合、すべての間隔の平均を計算
    // それ以外の場合、直近 N フレームの間隔の平均を計算
    void Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext, size_t recentCount = 0);

    // 平均時間をリセット
    // recentCount が 0 の場合、すべての間隔の平均を計算
    // それ以外の場合、直近 N フレームの間隔の平均を計算
    void Reset(ID3D11DeviceContext* deviceContext, size_t recentCount = 0);
    // コマンドキューに開始タイムスタンプを挿入
    HRESULT Start();
    // コマンドキューに終了タイムスタンプを挿入
    void Stop();
    // 間隔の取得を試み
    bool TryGetTime(double* pOut);
    // 強制的に間隔を取得（ブロッキングが発生する可能性あり）
    double GetTime();
    // 平均時間を計算
    double AverageTime()
    {
        if (m_RecentCount)
            return m_AccumTime / m_DeltaTimes.size();
        else
            return m_AccumTime / m_AccumCount;
    }
private:
    static bool GetQueryDataHelper(ID3D11DeviceContext* pContext, bool loopUntilDone, ID3D11Query* query, void* data, uint32_t dataSize);


    std::deque<double> m_DeltaTimes;    // 直近 N フレームのクエリ間隔
    double m_AccumTime = 0.0;           // クエリ間隔の累積合計
    size_t m_AccumCount = 0;            // 読み取り完了したクエリの回数
    size_t m_RecentCount = 0;           // 直近 N フレームを保持、0 の場合はすべてを含む

    std::deque<GpuTimerInfo> m_Queries; // 未完了のクエリをキャッシュ
    Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pImmediateContext;
};

#endif