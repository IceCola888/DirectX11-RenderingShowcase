#pragma once

#ifndef RENDER_STATES_H
#define RENDER_STATES_H

#include "WinMin.h"
#include <wrl/client.h>
#include <d3d11_1.h>


class RenderStates
{
public:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    static bool IsInit();

    static void InitAll(ID3D11Device* device);

public:
    static ComPtr<ID3D11RasterizerState> RSWireframe;		            // ラスタライザーステート：ワイヤーフレームモード
    static ComPtr<ID3D11RasterizerState> RSNoCull;			            // ラスタライザーステート：カリングなしモード
    static ComPtr<ID3D11RasterizerState> RSCullClockWise;	            // ラスタライザーステート：時計回りカリングモード
    static ComPtr<ID3D11RasterizerState> RSShadow;						// ラスタライザーステート：シャドウマップ用の深度バイアスモード

    static ComPtr<ID3D11SamplerState> SSPointClamp;						// サンプラーステート：ポイントフィルタリング + クランプモード
    static ComPtr<ID3D11SamplerState> SSLinearWrap;			            // サンプラーステート：リニアフィルタリング + ラップモード
    static ComPtr<ID3D11SamplerState> SSLinearClamp;					// サンプラーステート：リニアフィルタリング + クランプモード
    static ComPtr<ID3D11SamplerState> SSAnistropicWrap16x;		        // サンプラーステート：16倍異方性フィルタリング + ラップモード
    static ComPtr<ID3D11SamplerState> SSAnistropicClamp2x;		        // サンプラーステート：2倍異方性フィルタリング + クランプモード
    static ComPtr<ID3D11SamplerState> SSAnistropicClamp4x;		        // サンプラーステート：4倍異方性フィルタリング + クランプモード
    static ComPtr<ID3D11SamplerState> SSAnistropicClamp8x;		        // サンプラーステート：8倍異方性フィルタリング + クランプモード
    static ComPtr<ID3D11SamplerState> SSAnistropicClamp16x;		        // サンプラーステート：16倍異方性フィルタリング + クランプモード
    static ComPtr<ID3D11SamplerState> SSShadowPCF;						// サンプラーステート：シャドウマップ用のPCF + ボーダーモード


    static ComPtr<ID3D11BlendState> BSTransparent;		                // ブレンドステート：透明ブレンド
    static ComPtr<ID3D11BlendState> BSAlphaToCoverage;	                // ブレンドステート：アルファ・トゥ・カバレッジ
    static ComPtr<ID3D11BlendState> BSAdditive;			                // ブレンドステート：加算ブレンド
    static ComPtr<ID3D11BlendState> BSAlphaWeightedAdditive;            // ブレンドステート：アルファ加重の加算ブレンド


    static ComPtr<ID3D11DepthStencilState> DSSEqual;					// 深度/ステンシルステート：深度値が等しいピクセルのみ描画
    static ComPtr<ID3D11DepthStencilState> DSSLessEqual;                // 深度/ステンシルステート：従来のスカイボックス描画用
    static ComPtr<ID3D11DepthStencilState> DSSGreaterEqual;             // 深度/ステンシルステート：逆Zバッファ描画用
    static ComPtr<ID3D11DepthStencilState> DSSNoDepthWrite;             // 深度/ステンシルステート：深度テストのみ行い、書き込みなし
    static ComPtr<ID3D11DepthStencilState> DSSNoDepthTest;              // 深度/ステンシルステート：深度テスト無効
    static ComPtr<ID3D11DepthStencilState> DSSWriteStencil;		        // 深度/ステンシルステート：深度テストなし、ステンシル値を書き込む
    static ComPtr<ID3D11DepthStencilState> DSSEqualStencil;	            // 深度/ステンシルステート：逆Zバッファ、ステンシル値のチェック
};

#endif