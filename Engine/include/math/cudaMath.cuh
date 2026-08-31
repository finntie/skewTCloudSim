#pragma once


#include <cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <cmath>


struct float3x4
{
    float4 m[3];
};

inline __host__ __device__ float dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
inline __host__ __device__ float dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline __host__ __device__ float dot(float4 a, float4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }


inline __host__ __device__ float cross(float2 a, float2 b) { return a.x * b.y - a.y * b.x; }
inline __host__ __device__ float3 cross(float3 a, float3 b) { return make_float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

// transform Vector by matrix
inline __host__ __device__ float3 mul(const float3x4& mat, const float3& vector)
{ 
    float3 r;
    r.x = dot(vector, make_float3(mat.m[0].x, mat.m[0].y, mat.m[0].z));
    r.y = dot(vector, make_float3(mat.m[1].x, mat.m[1].y, mat.m[1].z));
    r.z = dot(vector, make_float3(mat.m[2].x, mat.m[2].y, mat.m[2].z));
    return r;
} 


// transform Vector by matrix with translation 
inline __host__ __device__ float4 mul(const float3x4& mat, const float4& vector)
{
    float4 r;
    r.x = dot(vector, mat.m[0]);
    r.y = dot(vector, mat.m[1]);
    r.z = dot(vector, mat.m[2]);
    r.w = 1.0f;
    return r;
}




inline __host__ __device__ float2 normalize(float2 v)
{
    float invLen = 1.0f / sqrtf(dot(v, v));
    return make_float2(v.x * invLen, v.y * invLen);
}
inline __host__ __device__ float3 normalize(float3 v)
{
    float invLen = 1.0f / sqrtf(dot(v, v));
    return make_float3(v.x * invLen, v.y * invLen, v.z * invLen);
}
inline __host__ __device__ float4 normalize(float4 v)
{
    float invLen = 1.0f / sqrtf(dot(v, v));
    return make_float4(v.x * invLen, v.y * invLen, v.z * invLen, v.w * invLen);
}



__device__ inline unsigned int rgbaFloatToInt(float4 rgba)
{
    return (unsigned(rgba.w * 255) << 24) | (unsigned(rgba.z * 255) << 16) | (unsigned(rgba.y * 255) << 8) |
           unsigned(rgba.x * 255);
}

inline __host__ __device__ float4 operator/(float4 a, float4 b) { return make_float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
inline __host__ __device__ float4 operator/(float4 a, float b) { return make_float4(a.x / b, a.y / b, a.z / b, a.w / b); }
inline __host__ __device__ float4 operator*(float4 a, float4 b) { return make_float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
inline __host__ __device__ float4 operator*(float4 a, float b) { return make_float4(a.x * b, a.y * b, a.z * b, a.w * b); }
inline __host__ __device__ float4 operator+(float4 a, float4 b) { return make_float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline __host__ __device__ float4 operator+(float4 a, float b) { return make_float4(a.x + b, a.y + b, a.z + b, a.w + b); }
inline __host__ __device__ float4 operator-(float4 a, float b) { return make_float4(a.x - b, a.y - b, a.z - b, a.w - b); }
inline __host__ __device__ float4 operator-(float a, float4 b) { return make_float4(a - b.x, a - b.y, a - b.z, a - b.w); }
inline __host__ __device__ float4 operator-(float4 a, float4 b) { return make_float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }

// Operators to make it easier to work with float3s
inline __host__ __device__ float3 operator+(float3 a, float3 b) { return make_float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline __host__ __device__ float3 operator+(float3 a, float b) { return make_float3(a.x + b, a.y + b, a.z + b); }
inline __host__ __device__ float3 operator-(float3 a, float3 b) { return make_float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline __host__ __device__ float3 operator-(float3 a, float b) { return make_float3(a.x - b, a.y - b, a.z - b); }
inline __host__ __device__ float3 operator-(float a, float3 b) { return make_float3(a - b.x, a - b.y, a - b.z); }
inline __host__ __device__ float3 operator*(float3 a, float3 b) { return make_float3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline __host__ __device__ float3 operator*(float3 a, float b) { return make_float3(a.x * b, a.y * b, a.z * b); }
inline __host__ __device__ float3 operator*(float b, float3 a) { return make_float3(a.x * b, a.y * b, a.z * b); }
inline __host__ __device__ float3 operator/(float3 a, float3 b) { return make_float3(a.x / b.x, a.y / b.y, a.z / b.z); }
inline __host__ __device__ float3 operator/(float3 a, float b) { return make_float3(a.x / b, a.y / b, a.z / b); }
inline __host__ __device__ float3 operator/(float a, float3 b) { return make_float3(a / b.x, a / b.y, a / b.z); }


inline __host__ __device__ float2 fabs2f(float2 a) { return make_float2(fabsf(a.x), fabsf(a.y)); }
inline __host__ __device__ float3 fabs3f(float3 a) { return make_float3(fabsf(a.x), fabsf(a.y), fabsf(a.z)); }

inline __host__ __device__ float2 ceil2f(float2 a) { return make_float2(ceilf(a.x), ceilf(a.y)); }
inline __host__ __device__ float3 ceil3f(float3 a) { return make_float3(ceilf(a.x), ceilf(a.y), ceilf(a.z)); }
inline __host__ __device__ float4 ceil4f(float4 a) { return make_float4(ceilf(a.x), ceilf(a.y), ceilf(a.z), ceilf(a.w)); }

inline __host__ __device__ float2 floor2f(float2 a) { return make_float2(floorf(a.x), floorf(a.y)); }
inline __host__ __device__ float3 floor3f(float3 a) { return make_float3(floorf(a.x), floorf(a.y), floorf(a.z)); }
inline __host__ __device__ float4 floor4f(float4 a) { return make_float4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w)); }

inline __host__ __device__ float clampf(float value, float min, float max) { return fmaxf(fminf(value, max), min); }
inline __host__ __device__ float2 clamp2f(float2 value, float min, float max)
{
    return make_float2(clampf(value.x, min, max), clampf(value.y, min, max));
}
inline __host__ __device__ float3 clamp3f(float3 value, float min, float max) 
{
    return make_float3(clampf(value.x, min, max), clampf(value.y, min, max), clampf(value.z, min, max));
}

inline __host__ __device__ float4 clamp4f(float4 value, float min, float max) 
{
    return make_float4(clampf(value.x, min, max),
                       clampf(value.y, min, max),
                       clampf(value.z, min, max),
                       clampf(value.w, min, max));
}

inline __host__ __device__ float2 expf2(float2 a) { return make_float2(expf(a.x), expf(a.y)); }
inline __host__ __device__ float3 expf3(float3 a) { return make_float3(expf(a.x), expf(a.y), expf(a.z)); }
inline __host__ __device__ float4 expf4(float4 a) { return make_float4(expf(a.x), expf(a.y), expf(a.z), expf(a.w)); }



// Integers

inline __host__ __device__ int3 operator+(int3 a, int3 b) { return make_int3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline __host__ __device__ int3 operator+(int3 a, int b) { return make_int3(a.x + b, a.y + b, a.z + b); }
inline __host__ __device__ int3 operator-(int3 a, int3 b) { return make_int3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline __host__ __device__ int3 operator-(int3 a, int b) { return make_int3(a.x - b, a.y - b, a.z - b); }
inline __host__ __device__ int3 operator-(int a, int3 b) { return make_int3(a - b.x, a - b.y, a - b.z); }
inline __host__ __device__ int3 operator*(int3 a, int3 b) { return make_int3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline __host__ __device__ int3 operator*(int3 a, int b) { return make_int3(a.x * b, a.y * b, a.z * b); }
inline __host__ __device__ int3 operator*(int b, int3 a) { return make_int3(a.x * b, a.y * b, a.z * b); }
inline __host__ __device__ int3 operator/(int3 a, int3 b) { return make_int3(a.x / b.x, a.y / b.y, a.z / b.z); }
inline __host__ __device__ int3 operator/(int3 a, int b) { return make_int3(a.x / b, a.y / b, a.z / b); }

// fmodf but clammed to positive values
inline __host__ __device__ float modClammed(float a, float b) {return fmodf(fmodf(a, b) + b, b); }


__inline__ float __host__ __device__ distance(float2 a)
{
    return sqrtf((a.x * a.x) + (a.y * a.y));
}

__inline__ float __host__ __device__ distance(float3 a)
{
    return sqrtf((a.x * a.x) + (a.y * a.y) + (a.z * a.z));
}

__inline__ float __host__ __device__ distance(float from, float to) { return fabsf(from - to); }

__inline__ float __host__ __device__ distance(float2 from, float2 to)
{
    float a = fabsf(from.x - to.x);
    float b = fabsf(from.y - to.y);
    return sqrtf((a * a) + (b * b)); 
}
__inline__ float __host__ __device__ distance(float3 from, float3 to)
{
    float a = fabsf(from.x - to.x);
    float b = fabsf(from.y - to.y);
    float c = fabsf(from.z - to.z);
    return sqrtf((a * a) + (b * b) + (c * c)); 
}

// Squared, still has to be rooted
__inline__ float __host__ __device__ distanceSquared(float3 from, float3 to)
{
    float a = fabsf(from.x - to.x);
    float b = fabsf(from.y - to.y);
    float c = fabsf(from.z - to.z);
    return (a * a) + (b * b) + (c * c);
}


__inline__ int __host__ __device__ DivideUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }


// Cubic interpolation with weight of w
__inline__ __host__ __device__ float interpolate(float a0, float a1, float w)
{
    return (a1 - a0) * (3.0f - w * 2.0f) * w * w + a0;
}

__inline__ __host__ __device__ float remap(float value, float low1, float high1, float low2, float high2)
{
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}


__inline__ __host__ __device__ unsigned int randomHash(unsigned int x, unsigned int y, unsigned int z, unsigned int seed)
{
    // Using Hash to get a random number
    unsigned int h = x * 374761393u + y * 668265263u + z * 1274126177u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return h;
}