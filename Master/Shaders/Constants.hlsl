#ifndef CONSTANTS_HLSL
#define CONSTANTS_HLSL

#define X_PI        3.141592653f
#define X_2PI       6.283185307f
#define X_PIDIV2    1.570796327f
#define X_PIDIV4    0.785398163f
#define X_INV_PI    0.318309886f
#define X_INV_2PI   0.159154943f

#define X_EPS       1e-6f

//--------------------------------------------------------------------------------------
// Tools
//--------------------------------------------------------------------------------------
// value を min_value と max_value の間で線形補間する
float linstep(float min_value, float max_value, float value)
{
    return saturate((value - min_value) / (max_value - min_value));
}

// 二乗関数
float Square(float x)
{
    return x * x;
}



#endif 