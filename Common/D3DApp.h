#pragma once
#ifndef D3DAPP_H
#define D3DAPP_H

#include <wrl/client.h>
#include <string>
#include <string_view>
#include "WinMin.h"
#include <d3d11_1.h>
#include <DirectXMath.h>
#include "CpuTimer.h"
#include "GpuTimer.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

class D3DApp
{
public:
    D3DApp(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight);
    virtual ~D3DApp();

    HINSTANCE AppInst()const;       // アプリインスタンス
    HWND      MainWnd()const;       // メインメウインドウ
    float     AspectRatio()const;   // スクリーン縦横比率

    int Run();                      // メッセージループ始まり

    // アプリケーション要件を実現するためにこれらのメソッドをオーバーライドする必要があります
    virtual bool Init();                      // ウインドウ、DirectX部分の初期化
    virtual void OnResize();                  // ウインドウ変更するときに呼びされる
    virtual void UpdateScene(float dt) = 0;   // フレームの更新
    virtual void DrawScene() = 0;             // フレームの描画
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); // ウィンドウメッセージコールバック

protected:
    bool InitMainWindow();      // ウインドウ初期化
    bool InitDirect3D();        // Direct3D初期化
    bool InitImGui();           // ImGui初期化

    void CalculateFrameStats(); // FPS表示
    ID3D11RenderTargetView* GetBackBufferRTV() { return m_pRenderTargetViews[m_FrameCount % m_BackBufferCount].Get(); }

protected:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    HINSTANCE m_hAppInst;        // アプリケーションインスタンスハンドル
    HWND      m_hMainWnd;        // メインウィンドウハンドル
    bool      m_AppPaused;       // 一時停止するかどうか
    bool      m_Minimized;       // 最小化されているか
    bool      m_Maximized;       // 最大化されているか
    bool      m_Resizing;        // ウィンドウのサイズが変更されたか

    bool m_IsDxgiFlipModel = false; // DXGIフリップモデルを使用するかどうか
    UINT m_BackBufferCount = 0;		// バックバッファの数
    UINT m_FrameCount = 0;          // 現在のフレーム
    ComPtr<ID3D11RenderTargetView> m_pRenderTargetViews[2];     // すべてのバックバッファに対応するレンダーターゲットビュー

    CpuTimer m_Timer;            // CPUタイマー

    // Direct3D 11
    ComPtr<ID3D11Device> m_pd3dDevice;                          // D3D11デバイス
    ComPtr<ID3D11DeviceContext> m_pd3dImmediateContext;	        // D3D11デバイスコンテキスト
    ComPtr<IDXGISwapChain> m_pSwapChain;                        // D3D11スワップチェーン
    // Direct3D 11.1
    ComPtr<ID3D11Device1> m_pd3dDevice1;			    		// D3D11.1デバイス
    ComPtr<ID3D11DeviceContext1> m_pd3dImmediateContext1;		// D3D11.1デバイスコンテキスト
    ComPtr<IDXGISwapChain1> m_pSwapChain1;						// D3D11.1スワップチェーン

    // 派生クラスはコンストラクタ内でこれらのカスタム初期パラメータを設定する必要がある
    std::wstring m_MainWndCaption;                              // メインウィンドウのタイトル
    int m_ClientWidth;                                          // ビューポートの幅
    int m_ClientHeight;                                         // ビューポートの高さ
};

#endif // D3DAPP_H