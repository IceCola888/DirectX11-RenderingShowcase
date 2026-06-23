#pragma once

#ifndef EFFECT_HELPER_H
#define EFFECT_HELPER_H

#include "WinMin.h"
#include <string_view>
#include <memory>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include "Property.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11GeometryShader;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;

//
// EffectHelper
//

// レンダリングパスの記述
// シェーダーを追加する際に指定した名前を使用してシェーダーを設定する
struct EffectPassDesc
{
    std::string_view nameVS;
    std::string_view nameDS;
    std::string_view nameHS;
    std::string_view nameGS;
    std::string_view namePS;
    std::string_view nameCS;
};


// 定数バッファの変数
// 非COMコンポーネント
struct IEffectConstantBufferVariable
{
    // 符号なし整数を設定、boolにも設定可能
    virtual void SetUInt(uint32_t val) = 0;
    // 符号付き整数を設定
    virtual void SetSInt(int val) = 0;
    // 浮動小数点数を設定
    virtual void SetFloat(float val) = 0;

    // 符号なし整数ベクターを設定、1～4コンポーネントを設定可能
    // シェーダー変数型がboolでも使用可能
    // 設定するコンポーネント数に応じてdataの先頭部分を読み取る
    virtual void SetUIntVector(uint32_t numComponents, const uint32_t data[4]) = 0;

    // 符号付き整数ベクターを設定、1～4コンポーネントを設定可能
    // 設定するコンポーネント数に応じてdataの先頭部分を読み取る
    virtual void SetSIntVector(uint32_t numComponents, const int data[4]) = 0;

    // 浮動小数点数ベクターを設定、1～4コンポーネントを設定可能
    // 設定するコンポーネント数に応じてdataの先頭部分を読み取る
    virtual void SetFloatVector(uint32_t numComponents, const float data[4]) = 0;

    // 符号なし整数行列を設定、行列のサイズは1～4まで許可
    // 渡されるデータはパディングなしである必要がある（例：3x3行列ならUINT[3][3]を直接渡せる）
    virtual void SetUIntMatrix(uint32_t rows, uint32_t cols, const uint32_t* noPadData) = 0;

    // 符号付き整数行列を設定、行列のサイズは1～4まで許可
    // 渡されるデータはパディングなしである必要がある（例：3x3行列ならINT[3][3]を直接渡せる）
    virtual void SetSIntMatrix(uint32_t rows, uint32_t cols, const int* noPadData) = 0;

    // 浮動小数点数行列を設定、行列のサイズは1～4まで許可
    // 渡されるデータはパディングなしである必要がある（例：3x3行列ならFLOAT[3][3]を直接渡せる）
    virtual void SetFloatMatrix(uint32_t rows, uint32_t cols, const float* noPadData) = 0;

    // その他の型を設定、設定範囲を指定可能
    virtual void SetRaw(const void* data, uint32_t byteOffset = 0, uint32_t byteCount = 0xFFFFFFFF) = 0;

    // プロパティを設定
    virtual void Set(const Property& prop) = 0;

    // 最後に設定された値を取得、取得範囲を指定可能
    virtual HRESULT GetRaw(void* pOutput, uint32_t byteOffset = 0, uint32_t byteCount = 0xFFFFFFFF) = 0;

    virtual ~IEffectConstantBufferVariable() {}
};


// レンダリングパス
// 非COMコンポーネント
class EffectHelper;
struct IEffectPass
{
    // ラスタライザーステートを設定
    virtual void SetRasterizerState(ID3D11RasterizerState* pRS) = 0;
    // ブレンドステートを設定
    virtual void SetBlendState(ID3D11BlendState* pBS, const float blendFactor[4], uint32_t sampleMask) = 0;
    // 深度ステンシルステートを設定
    virtual void SetDepthStencilState(ID3D11DepthStencilState* pDSS, uint32_t stencilValue) = 0;

    // 頂点シェーダーのuniformパラメータを取得し、値を設定
    virtual std::shared_ptr<IEffectConstantBufferVariable> VSGetParamByName(std::string_view paramName) = 0;
    // ドメインシェーダーのuniformパラメータを取得し、値を設定
    virtual std::shared_ptr<IEffectConstantBufferVariable> DSGetParamByName(std::string_view paramName) = 0;
    // ハルシェーダーのuniformパラメータを取得し、値を設定
    virtual std::shared_ptr<IEffectConstantBufferVariable> HSGetParamByName(std::string_view paramName) = 0;
    // ジオメトリシェーダーのuniformパラメータを取得し、値を設定
    virtual std::shared_ptr<IEffectConstantBufferVariable> GSGetParamByName(std::string_view paramName) = 0;
    // ピクセルシェーダーのuniformパラメータを取得し、値を設定
    virtual std::shared_ptr<IEffectConstantBufferVariable> PSGetParamByName(std::string_view paramName) = 0;
    // コンピュートシェーダーのuniformパラメータを取得し、値を設定
    virtual std::shared_ptr<IEffectConstantBufferVariable> CSGetParamByName(std::string_view paramName) = 0;
    // 所属するエフェクトヘルパーを取得
    virtual EffectHelper* GetEffectHelper() = 0;
    // エフェクト名を取得
    virtual const std::string& GetPassName() = 0;

    // シェーダー、定数バッファ（関数パラメータ含む）、サンプラー、シェーダーリソース
    // および読み書き可能リソースをレンダリングパイプラインに適用
    virtual void Apply(ID3D11DeviceContext* deviceContext) = 0;

    // コンピュートシェーダーをディスパッチ
    // スレッド数を指定すると、内部で適切なスレッドグループ数を計算して実行
    virtual void Dispatch(ID3D11DeviceContext* deviceContext, uint32_t threadX = 1, uint32_t threadY = 1, uint32_t threadZ = 1) = 0;

    virtual ~IEffectPass() {};
};




// シェーダー、サンプラー、シェーダーリソース、定数バッファ、シェーダーパラメータ
// 読み書き可能なリソース、レンダリングステートを管理
class EffectHelper
{
public:

    EffectHelper();
    ~EffectHelper();
    // コピー禁止、ムーブのみ許可
    EffectHelper(const EffectHelper&) = delete;
    EffectHelper& operator=(const EffectHelper&) = delete;
    EffectHelper(EffectHelper&&) = default;
    EffectHelper& operator=(EffectHelper&&) = default;

    // コンパイル済みシェーダーファイルのキャッシュパスを設定し作成
    // "" に設定するとキャッシュを無効化
    // forceWrite が true の場合、プログラム実行時に毎回強制的に上書き保存
    // デフォルトではコンパイル済みシェーダーをキャッシュしない
    // シェーダーの変更が完了していない場合は forceWrite を有効にすべき
    void SetBinaryCacheDirectory(std::wstring_view cacheDir, bool forceWrite = false);

    // シェーダーをコンパイルまたはシェーダーバイトコードを読み込み、以下の順序で処理：
    // 1. シェーダーバイトコードファイルのキャッシュパスが有効かつ強制上書きが無効の場合、
    //    まず `${cacheDir}/${shaderName}.cso` を読み込み、追加を試みる。
    // 2. それ以外の場合、`filename` を読み込む。シェーダーバイトコードなら直接追加。
    // 3. `filename` が HLSL ソースコードならコンパイルして追加。
    //    シェーダーバイトコードのキャッシュが有効なら、コンパイル結果を `${cacheDir}/${shaderName}.cso` に保存。
    // 注意：
    // 1. 異なるシェーダーコードで同じスロットを使用する定数バッファの定義は完全に一致させること
    // 2. 異なるシェーダーコードでグローバル変数が存在する場合、定義は完全に一致させること。
    // 3. 異なるシェーダーコードでサンプラー、シェーダーリソース、または可読書リソースが
    //    同じスロットを使用する場合、それらの定義を完全に一致させること。
    //    一致しない場合はスロットごとの設定のみ使用可能
    HRESULT CreateShaderFromFile(std::string_view shaderName, std::wstring_view filename, ID3D11Device* device,
        LPCSTR entryPoint = nullptr, LPCSTR shaderModel = nullptr, const D3D_SHADER_MACRO* pDefines = nullptr, ID3DBlob** ppShaderByteCode = nullptr);

    // シェーダーのみをコンパイル
    static HRESULT CompileShaderFromFile(std::wstring_view filename, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppShaderByteCode, ID3DBlob** ppErrorBlob = nullptr,
        const D3D_SHADER_MACRO* pDefines = nullptr, ID3DInclude* pInclude = D3D_COMPILE_STANDARD_FILE_INCLUDE);

    // コンパイル済みのシェーダーバイナリ情報を追加し、それに識別名を設定
    // この関数はシェーダーバイナリをファイルに保存しない
    // 注意:
    // 1. 異なるシェーダーコードであっても、同じregisterを使用する定数バッファの定義は完全に一致させる必要がある
    // 2. 異なるシェーダーコードであっても、グローバル変数の定義は完全に一致させる必要がある
    // 3. 異なるシェーダーコードであっても、同じスロットを使用するサンプラー、シェーダーリソース
    //    または読み書き可能なリソースの定義は完全に一致させる必要がある
    HRESULT AddShader(std::string_view name, ID3D11Device* device, ID3DBlob* blob);

    // ストリームアウト付きのジオメトリシェーダーを追加し、それに識別名を設定
    // この関数はシェーダーバイナリをファイルに保存しない
    // 注意:
    // 1. 異なるシェーダーコードであっても、同じスロットを使用する定数バッファの定義は完全に一致させる必要がある
    // 2. 異なるシェーダーコードであっても、グローバル変数の定義は完全に一致させる必要がある
    // 3. 異なるシェーダーコードであっても、同じスロットを使用するサンプラー、シェーダーリソース、または読み書き可能なリソースの定義は完全に一致させる必要がある
    //    一致しない場合、スロット単位での設定のみ可能
    HRESULT AddGeometryShaderWithStreamOutput(std::string_view name, ID3D11Device* device, ID3D11GeometryShader* gsWithSO, ID3DBlob* blob);

    // すべての内容をクリア
    void Clear();

    // レンダーパスを作成
    HRESULT AddEffectPass(std::string_view effectPassName, ID3D11Device* device, const EffectPassDesc* pDesc);
    // 特定のレンダーパスを取得
    std::shared_ptr<IEffectPass> GetEffectPass(std::string_view effectPassName);

    // 定数バッファの変数を取得し、値を設定
    std::shared_ptr<IEffectConstantBufferVariable> GetConstantBufferVariable(std::string_view name);

    // スロットごとにサンプラーステートを設定
    void SetSamplerStateBySlot(uint32_t slot, ID3D11SamplerState* samplerState);
    // 名前でサンプラーステートを設定（同じスロットに複数の名前がある場合はスロット指定のみ可能）
    void SetSamplerStateByName(std::string_view name, ID3D11SamplerState* samplerState);
    // 名前でサンプラーステートのスロットをマッピング（見つからない場合は-1を返す）
    int MapSamplerStateSlot(std::string_view name);

    // スロットごとにシェーダーリソースを設定
    void SetShaderResourceBySlot(uint32_t slot, ID3D11ShaderResourceView* srv);
    // 名前でシェーダーリソースを設定（同じスロットに複数の名前がある場合はスロット指定のみ可能）
    void SetShaderResourceByName(std::string_view name, ID3D11ShaderResourceView* srv);
    // 名前でシェーダーリソースのスロットをマッピング（見つからない場合は-1を返す）
    int MapShaderResourceSlot(std::string_view name);

    // スロットごとに読み書き可能リソースを設定
    void SetUnorderedAccessBySlot(uint32_t slot, ID3D11UnorderedAccessView* uav, uint32_t* pInitialCount = nullptr);
    // 名前で読み書き可能リソースを設定（同じスロットに複数の名前がある場合はスロット指定のみ可能）
    void SetUnorderedAccessByName(std::string_view name, ID3D11UnorderedAccessView* uav, uint32_t* pInitialCount = nullptr);
    // 名前で読み書き可能リソースのスロットをマッピング（見つからない場合は-1を返す）
    int MapUnorderedAccessSlot(std::string_view name);


    // デバッグオブジェクト名を設定
    void SetDebugObjectName(std::string name);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif