#pragma once

#include <glm/glm.hpp>

// Use of kernels, so hip include is fine.
#ifdef __CUDACC__
#include "msvc_defines.h"
#include <hip/hip_vector_types.h>
#endif

struct environmentData;
struct simParams;

void* getStream();

void initStream();

void initConstants(const float* invViewMatrix = nullptr, size_t sizeViewMat = 0, float3 gridMin = float3(), float3 gridMax = float3());

void fillSDF(glm::ivec3 gridSize,
             float* parameter,
             float densityTreshold,
             void* textureStorage,
             float* SDFClosestDist,
             int* SDFClosestTarget,
             dim3 gridDim,
             dim3 blockDim,
             void* stream);

__global__ void initJFASeeds(int3 size,
                             float* density,
                             float densityTreshold,
                             float* SDFClosestDist,
                             int* SDFClosestTarget,
                             const float invBlockSpread);

// Jump Flood Algorithm, fills SDFClosestDist with distances towards targets
__global__ void JFA(int3 size,
                    float* density,
                    float densityTreshold,
                    float* SDFClosestDist,
                    int* SDFClosestTarget,
                    const int3 offset,
                    const float invBlockSpread);

// Fills the LUTs that only need to be calculated ones, no need to reset
void fillLUTSOnce(environmentData& data, void* transmittanceLUT, void* scatteringLUT);

// If used, should ALWAYS be reset at least ones before closing.
void fillLUTS(environmentData& data,
              void* skyViewLUT,
              void* aerialViewLUT,
              unsigned int width,
              unsigned int height,
              bool reset);




/// <summary> Fill output with 4 layers of noise </summary>
/// <param name="output">Texture array that will be filled</param>
/// <param name="resolution">Resolution of each axis, can get expensive beyond 256/512</param>
/// <param name="samplePoints">Less points means less (and bigger) blobs for worley noise, more points is more (smaller) blobs</param>
/// <param name="layers">How many layers to add with potential different samplePoints</param>
/// <param name="increaseSamplePointsWithLayer">With how much samplePoints will increase per layer, normal values would be around 1.3 to 1.8 depended on #samplePoints</param>
/// <param name="seed">Initial seed</param>


// Noise, currently alligator
void fillNoiseTexture(float* output,
                      int resolution,
                      float persistence = 0.5f,
                      int gridSize = 4,
                      float lacunarity = 2.0f,
                      unsigned long long seed = 1);

void renderEnvironmentCUDA(simParams params,
                           dim3 blockSize,
                           unsigned int* dOutput,
                           environmentData data,
                           bool noDelay,
                           unsigned int width,
                           unsigned int height);

__global__ void renderEnvironmentCUDAGPU(unsigned int* dOutput,
                                         simParams params,
                                         environmentData data,
                                         unsigned int width,
                                         unsigned int height,
                                         int heightOffset);


