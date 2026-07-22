#include <cuda_runtime.h> 
#include <glm/glm.hpp>


struct environmentData;
struct CUstream_st;

void initStream();
cudaStream_t getStream();

void initConstants(const float* invViewMatrix = nullptr, size_t sizeViewMat = 0, float3 gridMin = float3(), float3 gridMax = float3());

void fillSDF(glm::ivec3 gridSize,
             float* parameter,
             void* textureStorage,
             float* SDFClosestDist,
             int* SDFClosestTarget,
             dim3 gridDim,
             dim3 blockDim,
             void* stream);

__global__ void initJFASeeds(int3 size,
                             float* density,
                             float* SDFClosestDist,
                             int* SDFClosestTarget,
                             const float invBlockSpread);

// Jump Flood Algorithm, fills SDFClosestDist with distances towards targets
__global__ void JFA(int3 size, float* density, float* SDFClosestDist, int* SDFClosestTarget, const int3 offset, const float invBlockSpread);

/// <summary> Fill output with 4 layers of noise </summary>
/// <param name="output">Texture array that will be filled</param>
/// <param name="resolution">Resolution of each axis, can get expensive beyond 256/512</param>
/// <param name="samplePoints">Less points means less (and bigger) blobs for worley noise, more points is more (smaller) blobs</param>
/// <param name="layers">How many layers to add with potential different samplePoints</param>
/// <param name="increaseSamplePointsWithLayer">With how much samplePoints will increase per layer, normal values would be around 1.3 to 1.8 depended on #samplePoints</param>
/// <param name="seed">Initial seed</param>
void fillNoiseTexture(float* output,
                      int resolution,
                      int octaves,
                      int gridSize = 4,
                      float lacunarity = 2.0f,
                      unsigned long long seed = 1);

void renderEnvironmentCUDA(dim3 gridSize,
                           dim3 blockSize,
                           unsigned int* dOutput,
                           environmentData data,
                           unsigned int width,
                           unsigned int height);

__global__ void renderEnvironmentCUDAGPU(unsigned int* dOutput,
                                         environmentData data,
                                         unsigned int width,
                                         unsigned int height,
                                         float heightOffset);

__device__ float intersectGrid(float3 dir, float3 origin, float3 recDir);

__device__ float calculateCloudCoverage(float3& pos, environmentData& data);

__device__ float calculateDensity(float3& pos, environmentData& data, const float cloudCoverage);

__device__ float lightMarch(float3 pos, const float3& lightDir, environmentData& data, float stepSize);

__device__ float henyenGreenstein(float inCosAngle, float inG);
