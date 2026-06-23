#pragma once

#ifndef CPU_TIMER_H
#define CPU_TIMER_H

class CpuTimer
{
public:
    CpuTimer();

    float TotalTime()const;     // Reset() 呼び出し後の経過時間を返す（ただし、停止期間は含まない）
    float DeltaTime()const;		// フレーム間の時間間隔を返す

    void Reset();               // 計測を開始する前またはリセットが必要なときに呼び出す
    void Start();               // 計測開始または一時停止解除時に呼び出す
    void Stop();                // 一時停止が必要なときに呼び出す
    void Tick();                // 各フレームの開始時に呼び出す
    bool IsStopped() const;     // タイマーが一時停止/終了しているかどうかを返す
private:
    double m_SecondsPerCount = 0.0;
    double m_DeltaTime = -1.0;

    __int64 m_BaseTime = 0;
    __int64 m_PausedTime = 0;
    __int64 m_StopTime = 0;
    __int64 m_PrevTime = 0;
    __int64 m_CurrTime = 0;

    bool m_Stopped = false;
};


#endif // CPUTIMER_H