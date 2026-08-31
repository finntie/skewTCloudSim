#include <cuda_runtime.h>

struct float3x4;

void setConstants(const float* invView = nullptr, size_t sizeViewMat = 0);

__device__ float posToLUT(const float3& pos, float width);

__device__ float dirToLUT(const float3& pos, const float3& dir, float height);

// Convert UV (0 to 1) values to zenith (angle from UP to SunDir) and azimuth (360 degrees compas angle) angles which are turned into a direction vector
__device__ float3 UVToDir(float u, float v, float3 up, float3 sunDir, float r);

__device__ float2 dirToUV(float3 dir, float3 pos, float3 sunDir);

__device__ float3 envTransDir(const float3& pos, const float3& dir, unsigned long long TTexture);

__device__ float3 envTrans(const float3& pos, const float3& pos2, unsigned long long TTexture);

__device__ float3 envScat(const float3& pos, const float3& lightDir, unsigned long long TScat);

__device__ float3 envPhaseFunction(float angle, float g);

__device__ float3 shadowTerm(const float3& pos, const float3& lightdir, unsigned long long TTexture);

__device__ float3 groundRadiance(const float3& pos, const float3& lightdir, unsigned long long TTexture);

__device__ float3 getIpoint(const float3& pos, const float3& dir, bool& hitGround);

__global__ void atmosphericTransmittance(float4* transmittance, int2 resolution);

__device__ float3 sphereSamplePoint(float u1, float u2);

__device__ float3 transferFunction(const float3& pos, const float3& lightdir, unsigned long long TTexture);

__global__ void envScattering(float4* scattering, float2 resolution, unsigned long long TTexture);

__device__ float3 luminanceFunction(const float3& from,
                                    const float3& to,
                                    const float3& dir,
                                    const float3& lightDir,
                                    unsigned long long TTexture,
                                    unsigned long long TScattering);
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


__device__ float4 getAtmosphericSkyView(float3 dir, float3 pos, float3 lightDir, float2 resolution, unsigned long long skyViewTexture);