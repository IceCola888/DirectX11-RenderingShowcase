#ifndef SHADER_DEFINES_H
#define SHADER_DEFINES_H

// 最大ライト数（2^12 = 4096）
#define MAX_LIGHTS_POWER 12
#define MAX_LIGHTS (1 << MAX_LIGHTS_POWER)
// タイルあたりの最大ライトインデックス数
#define MAX_LIGHT_INDICES ((MAX_LIGHTS >> 6) - 1)

// タイルベースライティング・コンピュートシェーダーのタイルサイズ
#define COMPUTE_SHADER_TILE_GROUP_DIM 16
#define COMPUTE_SHADER_TILE_GROUP_SIZE (COMPUTE_SHADER_TILE_GROUP_DIM * COMPUTE_SHADER_TILE_GROUP_DIM)

// サンプル単位のシェーディングを遅延実行して、SIMD並列化を最適化する
#define DEFER_PER_SAMPLE 1

#endif