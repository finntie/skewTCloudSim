#pragma once


#include "math/cudaMath.cuh"
// Use of kernels, so hip include is fine.
#ifdef __CUDACC__
#include "msvc_defines.h"
#include <hip/hip_runtime.h>
#include <hip/hip_vector_types.h>
#endif

struct float3x4;

void setConstants(const float* invView = nullptr, size_t sizeViewMat = 0, void* stream = nullptr);


__global__ void atmosphericTransmittance(float4* transmittance, int2 resolution);



__global__ void envScattering(float4* scattering, float2 resolution, unsigned long long TTexture);


__global__ void envSkyView(float4* skyView,
                           const float3 lightDir,
                           const int2 resolution,
                           unsigned long long TTexture,
                           unsigned long long TScat);

__global__ void envAerialView(float4* aerialView,
                              const float2 screenSize,
                              const float3 lightDir,
                              const int3 resolution,
                              unsigned long long TTexture,
                              unsigned long long TScat);

// Declared in header due to device kernel
__device__ inline float2 dirToUV(float3 dir, float3 pos, float3 sunDir)
{
    // Created from CLAUDE AI
    // Reverse of function above
    constexpr float radiusEarth = 6360.0f;
    pos = pos + make_float3(0, radiusEarth, 0);

    const float r = distance(pos);
    const float3 up = pos / r;

    const float cosBeta = sqrt(fmaxf(r * r - radiusEarth * radiusEarth, 0.0f)) / r;
    const float beta = acosf(clampf(cosBeta, -1.0f, 1.0f));
    const float zenithHorizon = 3.1415926 - beta;

    // Get the zenith angle of the direction (vertical angle)
    const float viewZenith = acosf(clampf(dot(dir, up), -1.0f, 1.0f));

    float v = 0.0f;
    // Inverse of our little remapping trick
    if (viewZenith < zenithHorizon) v = 0.5f * (1.0f - sqrtf(fmaxf(1.0f - viewZenith / zenithHorizon, 0.0f)));
    else v = 0.5f * (1.0f - sqrtf(fmaxf((viewZenith - zenithHorizon) / beta, 0.0f)));

    // Remove up direction from the sun and normal direction
    float3 sunT = sunDir - up * dot(sunDir, up);
    float3 viewT = dir - up * dot(dir, up);
    const float ls = distance(sunT);
    const float lv = distance(viewT);
    // If any of the direction is almost fully up, we can not divide with it, so cos will be 1.0
    const float lightViewCos = (ls < 1e-4f || lv < 1e-4f) ? 1.0f : clampf(dot(sunT / ls, viewT / lv), -1.0f, 1.0f);

    // Map u from 0 to 1, and unmap the squaring we did
    const float u = sqrt(fmaxf(0.5f * (1.0f - lightViewCos), 0.0f));

    return make_float2(u, v);
}

// Declared in header due to device kernel
__device__ inline float4 getAtmosphericSkyView(float3 dir, float3 pos, float3 lightDir, float2 resolution, unsigned long long skyViewTexture)
{
    pos = pos / 1000.0f; // Make sure 1000.0f coordinate is 1 km and not 1000 km
   
    float2 uv = dirToUV(dir, pos, lightDir);

    // Unmap texel center correction
    const float s = uv.x * (1.0f - 1.0f / resolution.x) + 0.5f / resolution.x;
    const float t = uv.y * (1.0f - 1.0f / resolution.y) + 0.5f / resolution.y;

    // Thats all    
    return tex2D<float4>(skyViewTexture, s, t);
}
