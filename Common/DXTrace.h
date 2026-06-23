#pragma once

#ifndef DXTRACE_H
#define DXTRACE_H

#include "WinMin.h"

// ------------------------------
// DXTraceW関数
// ------------------------------
// デバッグ出力ウィンドウにフォーマットされたエラー情報を出力し、
// 必要に応じてエラーダイアログを表示
// [In]strFile			現在のファイル名（通常、マクロ __FILEW__ を渡す）
// [In]hlslFileName     現在の行番号（通常、マクロ __LINE__ を渡す）
// [In]hr				関数の実行中に発生した問題の HRESULT 値
// [In]strMsg			デバッグ時の問題特定を助ける文字列（通常 L#x を渡すことがある、NULL 可能）
// [In]bPopMsgBox       TRUE の場合、エラーメッセージボックスを表示
// 戻り値: 引数 hr
HRESULT WINAPI DXTraceW(_In_z_ const WCHAR* strFile, _In_ DWORD dwLine, _In_ HRESULT hr, _In_opt_ const WCHAR* strMsg, _In_ bool bPopMsgBox);


// ------------------------------
// HRマクロ
// ------------------------------
// Debug モードでのエラー警告とトレース
#if defined(DEBUG) | defined(_DEBUG)
#ifndef HR
#define HR(x)												\
    {															\
        HRESULT hr = (x);										\
        if(FAILED(hr))											\
        {														\
            DXTraceW(__FILEW__, (DWORD)__LINE__, hr, L#x, true);\
        }														\
    }
#endif
#else
#ifndef HR
#define HR(x) (x)
#endif 
#endif



#endif