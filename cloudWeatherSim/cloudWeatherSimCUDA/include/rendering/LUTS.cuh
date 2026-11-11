#pragma once


#include "math/cudaMath.cuh"
// Use of kernels, so hip include is fine.
#include "gpu_defines.h"
#ifdef GPU_COMPILER
#include "msvc_defines.h"
#include <hip/hip_runtime.h>
#include <hip/hip_vector_types.h>
#endif

struct float3x4;

// Atmospheric rendering based on https://sebh.github.io/publications/egsr2020.pdf

void setConstants(const float* invView = nullptr, size_t sizeViewMat = 0, void* stream = nullptr);


__global__ void atmosphericTransmittance(float4* transmittance, int2 resolution);



__global__ void envScattering(float4* scattering, float2 resolution, unsigned long long TTexture);


__global__ void envSkyView(float4* skyView,
                           const float3 lightDir,
                           const int2 resolution,
                           unsigned long long TTexture,
                           unsigned long long TScat);

__global__ void envAerialView(float4* aerialView,
                              const float3 lightDir,
                              const int3 resolution,
                              unsigned long long TTexture,
                              unsigned long long TScat);

// Calculates the intersectionpoint of the ray, if it hit the ground or space
__device__ inline float3 getIpoint(const float3& pos, const float3& dir, bool& hitGround)
{

    float radiusEarth = 6360.0f;
    float sampleHeight = 100.0f;

    // Calculate intersection point with space or ground.
    // Using the ray formula: P = O + D * t
    // And the circle formula P^2 = r^2
    // With P being a 3D point in space and the circle center being at (0,0,0)
    // Using the quadratic formula with intersection of a ray and circle (noting that D*D = 1):
    // t^2 + t * (2(O*D)) + (O*O) - r^2 = 0
    // With which we get:  A = 1,  B = 2(O*D),  C = (O*O) - r^2

    const float3 O = make_float3(pos.x, pos.y + radiusEarth, pos.z);
    const float3 D = normalize(dir); // Making sure
    float R = radiusEarth;
    hitGround = false;

    // The ABC formula then goes: t = (-b +- sqrt(b^2 - 4ac)) / 2a
    // Yet, we can simplify due to a being 1 and b being 2 * dot(O, D)
    // Imagine b being dot(O,D) then we must put a 2 in front: 2b
    // t = (-2b +- sqrt(4b^2 - 4c)) / 2
    // Multiply sqrt by 2, being able to divide the values inside the sqrt by 4:
    // t = (-2b +- 2 * sqrt(b^2 - c)) / 2
    // Remove the division by multiplying:
    // t = -b +- sqrt(b^2 - c)

    const float b = dot(O, D); // Not multiplying by 2, since we divided this away

    const float r = distance(O);
    float h = r - R;
    float c = h * (2.0f * R + h); // Same as dot(O,O) - R * R, but uses smaller numbers, meaning no floating point issues

    float disc = b * b - c;

    // If the discriminant not negative, we hit the ground
    if (disc >= 0.0f)
    {
        const float t = -b - sqrt(disc);
        // If t is positive, we hit the ground forward, else we hit it behind us and we dont care
        if (t >= 0.0f)
        {
            hitGround = true;
            return O + D * t;
        }
    }

    // Instead of the ground, we will now look at intersection with space
    // This must hit, since the position must be inside the circle
    R = radiusEarth + sampleHeight;

    h = r - R;
    c = h * (2.0f * R + h);  // Same as dot(O,O) - R * R, but uses smaller numbers, meaning no floating point issues
    disc = b * b - c;
    if (disc < 0.0f) return make_float3(0, 0, 0);
    const float t = -b + sqrt(disc);

    float3 result = O + D * t;
    return result;
}

// Declared in header due to device kernel
__device__ inline float2 dirToUV(float3 dir, float3 pos, float3 sunDir)
{
    // Created from CLAUDE AI
    // Reverse of UVToDir function in LUTS.cu
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
__device__ __inline__ float4 getAtmosphericSkyView(float3 dir, float3 pos, float3 lightDir, float2 resolution, unsigned long long skyViewTexture)
{
    pos = pos / 1000.0f; // Make sure 1000.0f coordinate is 1 km and not 1000 km
   
    float2 uv = dirToUV(dir, pos, lightDir);

    // Unmap texel center correction
    const float s = uv.x * (1.0f - 1.0f / resolution.x) + 0.5f / resolution.x;
    const float t = uv.y * (1.0f - 1.0f / resolution.y) + 0.5f / resolution.y;

    // Thats all    
    return tex2D<float4>(toTexObj(skyViewTexture), s, t);
}

// Depth in meters, resolution.z in kilometers being max, above will be set to resolution.z km.
__device__ __inline__ float4 getAtmosphericAerialView(float3 dir, float3 pos, float depth, float3 lightDir, int3 resolution, unsigned long long aerialViewTexture, bool debug)
{
    pos = pos / 1000.0f; // Make sure 1000.0f coordinate is 1 km and not 1000 km
    depth = depth / 1000.0f;

    float2 uv = dirToUV(dir, pos, lightDir);

    // Unmap texel center correction
    const float s = uv.x * (1.0f - 1.0f / float(resolution.x)) + 0.5f / float(resolution.x);
    const float t = uv.y * (1.0f - 1.0f / float(resolution.y)) + 0.5f / float(resolution.y);

    float slice = depth / 100.0f;
    float weight = 1.0f;
    // If beneath 0.5 of the first slice, set back to 0.5f to prevent issues.
    if (slice < 0.5f) { weight = slice * 2.0f; slice = 0.5f; }
    float w = slice / float(resolution.z); // New depth

    float4 result = tex3D<float4>(toTexObj(aerialViewTexture), s, t, w);
    // When close, just reduce the scattering
    result = make_float4(result.x * weight, result.y * weight, result.z * weight, result.w);
    // Small lerp when close to ground to get smoother result.
    result.w = (1 - weight) + result.w * weight;

    if (debug)
    {
        printf("UV: %f, %f, s %f t %f, dir: %f, %f, %f depth %f\n",
            uv.x,
            uv.y,
            s,
            t,
            dir.x, dir.y, dir.z,
            depth);
    }

    return result;  
}