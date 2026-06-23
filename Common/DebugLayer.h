#pragma once

#ifndef DEBUG_LAYER_H
#define DEBUG_LAYER_H

#include "WinMin.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <vector>
#include <deque>
#include <string>


class DebugLayer
{
public:
    ~DebugLayer() { ClearMessages(); }

    HRESULT Init(ID3D11Device* device);

    // 存在するオブジェクトを報告する 
    void ReportLiveDeviceObjects(D3D11_RLDO_FLAGS detailLevel) { m_pDebug->ReportLiveDeviceObjects(detailLevel); }

    // デバッグ出力ウィンドウへのメッセージ出力を無効にするかどうか
    void MuteDebugOutput(bool mute) { m_pInfoQueue->SetMuteDebugOutput(mute); }

    // 情報キュー内のすべてのメッセージをキャッシュし、ID3D11InfoQueue内のメッセージをクリアする  
    const std::vector<D3D11_MESSAGE*>& FetchMessages();

    // キャッシュされたすべてのメッセージをクリアする
    void ClearMessages();


private:
    Microsoft::WRL::ComPtr<ID3D11Debug> m_pDebug;
    Microsoft::WRL::ComPtr<ID3D11InfoQueue> m_pInfoQueue;
    std::vector<D3D11_MESSAGE*> m_pMessages;
};

#endif