#pragma once

#include "platform/cuda/cuda_render.cuh"
#include "platform/cuda/cuda_render_gl.h"
#include "platform/cuda/texture_noise.cuh"
#include "platform/cuda/LUTS.cuh"

#include <cuda_runtime.h>
#include <curand_kernel.h>
#include "math/cudaMath.cuh"

#include <iostream>
#include <atomic>



// view Matrix
__constant__ float3x4 invView; 
__constant__ float3 gridMin;
__constant__ float3 gridMax;

__constant__ float QWMIN = 0.0001f;
__constant__ float QWMAX = 0.005f;
__constant__ float QRMIN = 0.0001f;
__constant__ float QRMAX = 0.005f;
__constant__ float QSMIN = 0.0001f;
__constant__ float QSMAX = 0.005f;

// Stream
cudaStream_t renderStream;

struct Ray
{
    float3 O{};
    float3 D{};
    float3 rD{};
};


__inline__ __device__ bool isOutside(float x, float y, float z)
{
    return (x + 1 > gridMax.x) | (x < gridMin.x) | 
            (y + 1 > gridMax.y) | (y < gridMin.y) | 
            (z + 1 > gridMax.z) |(z < gridMin.z);
}

void initStream() 
{ 
    int leastPriority, greatestPriority;
    cudaDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);
    cudaStreamCreateWithPriority(&renderStream, cudaStreamNonBlocking, leastPriority); 
}

cudaStream_t getStream() 
{ 
    return renderStream; 
}

void initConstants(const float* invViewMatrix, size_t sizeViewMat, float3 _gridMin, float3 _gridMax)
{
    if (invViewMatrix)
    {
        cudaMemcpyToSymbol(invView, invViewMatrix, sizeViewMat);
        setConstants(invViewMatrix, sizeViewMat);
    }
    cudaMemcpyToSymbol(gridMin, &_gridMin, sizeof(float3));
    cudaMemcpyToSymbol(gridMax, &_gridMax, sizeof(float3));
}

void fillSDF(glm::ivec3 gridSize, float* parameter, float densityTreshold, void* textureStorage, float* SDFClosestDist, int* SDFClosestTarget, dim3 gridDim, dim3 blockDim, void* stream)
{
    // Fill Signed Distance Field using Jump Flood Algorithm
    const float invBlockSpreadDepth = 1.0f / (float(gridDim.z) / float(gridSize.z));
    int3 size = make_int3(gridSize.x, gridSize.y, gridSize.z);

    // Initialize the seeds (current indices) making sure that at a target the index is correctly set.
    initJFASeeds<<<gridDim, blockDim, 0, static_cast<cudaStream_t>(stream)>>>(size,
                                                                              parameter,
                                                                              densityTreshold,
                                                                              SDFClosestDist,
                                                                              SDFClosestTarget,
                                                                              invBlockSpreadDepth);

    int3 offset = make_int3(gridSize.x, gridSize.y, gridSize.z);

    // Update signed distance field by reducing offset by half
    // We will check each neighbour on all axis with this offset
    // If this is a target, we check distance to it and update closest distance
    // We also check the data of this neighbour to see if they have a closer target to us
    while (true)
    {
        // Half offset
        offset.x = int(ceilf(float(offset.x) / 2.0f));
        offset.y = int(ceilf(float(offset.y) / 2.0f));
        offset.z = int(ceilf(float(offset.z) / 2.0f));

        JFA<<<gridDim, blockDim, 0, static_cast<cudaStream_t>(stream)>>>(size,
                                                                         parameter,
                                                                         densityTreshold,
                                                                         SDFClosestDist,
                                                                         SDFClosestTarget,
                                                                         offset,
                                                                         invBlockSpreadDepth);

        if (offset.x <= 1 && offset.y <= 1 && offset.z <= 1) break;
    }

    cudaMemcpy3DParms cpyParams{};
    cpyParams.srcPtr = make_cudaPitchedPtr(SDFClosestDist, size.x * sizeof(float), size.x, size.y);
    cpyParams.dstArray = static_cast<cudaArray_t>(textureStorage);
    cpyParams.extent = make_cudaExtent(size.x, size.y, size.z);
    cpyParams.kind = cudaMemcpyDeviceToDevice;
    cudaMemcpy3DAsync(&cpyParams, static_cast<cudaStream_t>(stream));
}

__global__ void initJFASeeds(int3 size,
                             float* density,
                             float densityTreshold,
                             float* SDFClosestDist,
                             int* SDFClosestTarget,
                             const float invBlockSpread)
{
    const int x = threadIdx.x + blockDim.x * blockIdx.x;
    const int y = threadIdx.y + blockDim.y * blockIdx.y;
    int z = int(float(blockIdx.z) * invBlockSpread);  // Get z index from spread and block index on z dimension.

    if (x >= size.x || y >= size.y || z >= size.z) return;



    for (; z < fminf(size.z, ceilf(float(blockIdx.z + 1) * invBlockSpread)); z++)
    {
        const int idx = x + y * size.x + z * size.x * size.y;

        SDFClosestDist[idx] = 1e6f;
        SDFClosestTarget[idx] = -1;

        if (density[idx] > densityTreshold)
        {
            SDFClosestDist[idx] = 0.0f;
            SDFClosestTarget[idx] = idx;
        }
    }
}

__global__ void JFA(int3 size,
                    float* density,
                    float densityTreshold,
                    float* SDFClosestDist,
                    int* SDFClosestTarget,
                    const int3 offset,
                    const float invBlockSpread)
{ 
    const int x = threadIdx.x + blockDim.x * blockIdx.x;
    const int y = threadIdx.y + blockDim.y * blockIdx.y;
    int z = int(float(blockIdx.z) * invBlockSpread);  // Get z index from spread and block index on z dimension.

    if (x >= size.x || y >= size.y || z >= size.z) return;

    // We just grab the Z for the next block index and loop until there
    for (; z < fminf(size.z, ceilf(float(blockIdx.z + 1) * invBlockSpread)); z++)
    {
        const int idx = x + y * size.x + z * size.x * size.y;

        float currentClosestDist = SDFClosestDist[idx] /** (float(fmaxf(size.x, fmaxf(size.y, size.z))))*/;
        currentClosestDist *= currentClosestDist; // Square distance, making it faster to check
        int closestTarget = -1;


        // Loop over all neighbours
        for (int zi = -offset.z + z; zi <= offset.z + z; zi += offset.z)
        {
            for (int yi = -offset.y + y; yi <= offset.y + y; yi += offset.y)
            {
                for (int xi = -offset.x + x; xi <= offset.x + x; xi += offset.x)
                {
                    if (xi < 0 || yi < 0 || zi < 0 || xi >= size.x || yi >= size.y || zi >= size.z) 
                    {
                        continue;
                    }

                    int nIdx = xi + yi * size.x + zi * size.x * size.y;
                    const int nTarget = SDFClosestTarget[nIdx];
                    
                    // Check if its a target
                    if (density[nIdx] > densityTreshold)
                    {
                        // Calculate distance to target and save data if its closest
                        const float dist = distanceSquared(make_float3(x, y, z), make_float3(xi, yi, zi));
                        if (dist < currentClosestDist)
                        {
                            currentClosestDist = dist;
                            closestTarget = nIdx;
                        }
                    }
                    

                    // If neighbour has a valid target already
                    if (nTarget >= 0) 
                    {
                        const int xy = nTarget % (size.x * size.y);
                        const float tx = xy % size.x;
                        const float ty = (xy - tx) / size.x;
                        const float tz = (nTarget - xy) / (size.x * size.y);

                        // Compare against neighbour target
                        float dist = distanceSquared(make_float3(x, y, z), make_float3(tx, ty, tz));
                        if (dist < currentClosestDist)
                        {
                            currentClosestDist = dist;
                            closestTarget = nTarget;
                        }
                    }

                    //if (x == size.x / 2 && y == size.y / 2 && z == size.z / 2)
                    //{
                    //    printf("x %i y %i z %i, xi %i yi %i zi %i, nTarget %i, currentClosestDist %f, closestTarget, %i\n",
                    //           x,
                    //           y,
                    //           z,
                    //           xi,
                    //           zi,
                    //           yi,
                    //           nTarget,
                    //           currentClosestDist,
                    //           closestTarget);
                    //}
                }
            }
        }

        if (closestTarget != -1)
        {
            SDFClosestDist[idx] = sqrt(currentClosestDist)/* / (float(fmaxf(size.x, fmaxf(size.y, size.z))))*/; // We still have to root the distance
            SDFClosestTarget[idx] = closestTarget;
        }
    }
}

void fillLUTSOnce(environmentData& data,
                  void* transmittanceLUT,
                  void* scatteringLUT)
{
    dim3 blockTrans{16, 16};
    dim3 gridTrans{256 / blockTrans.x, 64 / blockTrans.y};

    float4* tempArray4;
    cudaMalloc((void**)&tempArray4, 256 * 64 * sizeof(float4));


    atmosphericTransmittance<<<gridTrans, blockTrans, 0, renderStream>>>(tempArray4, make_int2(256, 64));
    copyDataToTexture<float4>(tempArray4, transmittanceLUT, glm::ivec3(256, 64, 0), renderStream);
    cudaFreeAsync(tempArray4, renderStream);
    cudaMalloc((void**)&tempArray4, 32 * 32 * sizeof(float4));
    gridTrans = dim3(32 / blockTrans.x, 32 / blockTrans.y);
    envScattering<<<gridTrans, blockTrans, 0, renderStream>>>(tempArray4, make_float2(32, 32), data.envTransmittanceTexture);
    copyDataToTexture<float4>(tempArray4, scatteringLUT, glm::ivec3(32, 32, 0), renderStream);
    cudaFreeAsync(tempArray4, renderStream);

            cudaStreamSynchronize(renderStream);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }
}


inline int iDivUp(int a, int b) { return (a % b != 0) ? (a / b + 1) : (a / b); }

void fillLUTS(environmentData& data, void* skyViewLUT, void* aerialViewLUT, unsigned int width, unsigned int height, bool reset) 
{
    static float4* tempData = nullptr;
    static float4* tempData3D = nullptr;
    static bool initialized = false;

    if (reset && initialized)
    {
        if (!initialized)
        {
            cudaFree(tempData);
            cudaFree(tempData3D);
        }
        return;
    }

    if (!initialized)
    {
        cudaMalloc((void**)&tempData, 200 * 100 * sizeof(float4));
        cudaMalloc((void**)&tempData3D, 32 * 32 * 32 * sizeof(float4));
        initialized = true;
    }

    const float3 lightDir = normalize(make_float3(data.sunDirection[0] + 1e-6f, data.sunDirection[1] + 1e-6f, data.sunDirection[2] + 1e-6f));

    int2 skyViewRes = make_int2(200, 100);
    int3 aerialViewRes = make_int3(32, 32, 32);

    dim3 blockSize{16, 16};
    dim3 gridSize{unsigned(iDivUp(skyViewRes.x, blockSize.x)), unsigned(iDivUp(skyViewRes.y, blockSize.y))};

    // Calculate the data and put it in the data
    envSkyView<<<gridSize, blockSize, 0, renderStream>>>(tempData,
                                      lightDir,
                                      skyViewRes,
                                      data.envTransmittanceTexture,
                                      data.envScatteringTexture);

    // Make the dimension 3D
    blockSize = dim3(8, 8, 8);
    gridSize = dim3(unsigned(iDivUp(aerialViewRes.x, blockSize.x)),
                  unsigned(iDivUp(aerialViewRes.y, blockSize.y)),
                  unsigned(iDivUp(aerialViewRes.z, blockSize.z)));

    //envAerialView<<<gridSize, blockSize, 0, renderStream>>>(tempData3D,
    //                                       make_float2(width, height),
    //                                       lightDir,
    //                                       aerialViewRes,
    //                                       data.envTransmittanceTexture,
    //                                       data.envScatteringTexture);


    copyDataToTexture<float4>(tempData, skyViewLUT, glm::ivec3(200, 100, 0), renderStream);
    //copyDataToTexture<float4>(tempData3D, aerialViewLUT, glm::ivec3(32, 32, 32), renderStream);

    cudaStreamSynchronize(renderStream);
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }
}





void fillNoiseTexture(float* output,
                      int resolution,
                      int octaves,
                      int _gridSize,
                      float lacunarity, 
                      unsigned long long seed)
{
    dim3 blockSize(8, 8, 4);
    dim3 gridSize =
        dim3(DivideUp(resolution, blockSize.x), DivideUp(resolution, blockSize.y), DivideUp(resolution, blockSize.z));


    alligatorNoise<<<gridSize, blockSize, 0, renderStream>>>(output, resolution, _gridSize, seed, octaves, lacunarity, 0.5f);


    //int increasingSamplePoints = samplePoints;
    //float contribution = 1.0f;

    //for (int i = 0; i < layers; i++)
    //{

    //    generateWorley<<<gridSize, blockSize, 0, renderStream>>>(output,
    //                                                             resolution,
    //                                                             increasingSamplePoints,
    //                                                             contribution,
    //                                                             seed);

    //    // Increase amount of sample points per layer, creating for more texture, early return if sample points become too large
    //    increasingSamplePoints = int(float(increasingSamplePoints) * increaseSamplePointsWithLayer);
    //    contribution *= 0.5f;
    //    if (increasingSamplePoints >= resolution) break;
    //}
    //combineWithPerlin<<<gridSize, blockSize, 0, renderStream>>>(output, resolution, seed);

}

void renderEnvironmentCUDA(dim3 gridSize,
                           dim3 blockSize,
                           unsigned int* dOutput,
                           environmentData data,
                           unsigned int width,
                           unsigned int height)
{
    // Split up work
    int pixelsPer = 64;
    for (int i = 0; i < height; i += pixelsPer)
    {
        int h = std::min(pixelsPer, int(height) - i);
        dim3 newGrid(DivideUp(width, blockSize.x), DivideUp(h, blockSize.y));
        renderEnvironmentCUDAGPU<<<newGrid, blockSize, 0, renderStream>>>(dOutput, data, width, height, i);
    cudaStreamSynchronize(renderStream); // Synchronize to give simulation time to also do their part

        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
        {
            std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
            __debugbreak();
        }
    }

}

__global__ void renderEnvironmentCUDAGPU(unsigned int* dOutput,
                                         environmentData data,
                                         unsigned int width,
                                         unsigned int height,
                                         int heightOffset)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y + heightOffset;

    // Make sure its valid and within
    if (x >= width || y >= height) return;

    // Randomizer
    curandState state;
    curand_init(100, 0, 0, &state);
    unsigned int rSeed = x + y * width;

    // Calculation of fov and how this would effect the focalLength
    // With focalLength being the distance from the camera origin to the virtual image plane
    // The further away the focalLenght is, the more zoomed in the image
    float aspectRatio = float(width) / float(height);
    float fov = 60.0f;
    float focalLength = 1.0f / tanf(fov * 0.5f * 3.1415926f / 180.0f);

    // Set UV, ranging between -1 and 1
    float u = (((float(x) + 0.5f) / float(width)) * 2.0f - 1.0f) * aspectRatio;
    float v = (((float(y) + 0.5f) / float(height)) * 2.0f - 1.0f);

    // Add a nice light direction
    const float3 lightDir =
        normalize(make_float3(data.sunDirection[0] + 1e-6f, data.sunDirection[1] + 1e-6f, data.sunDirection[2] + 1e-6f));
    // Color the light
    const float4 lightColor = make_float4(data.sunColor[0], data.sunColor[1], data.sunColor[2], 1.0f);
    const float sunStrength = data.sunStrength;

    // Create ray and set origin + direction
    Ray mainR;
    float4 Otemp = mul(invView, make_float4(0.0f, 0.0f, 0.0f, 1.0f));
    mainR.O = make_float3(Otemp.x, Otemp.y, Otemp.z);
    mainR.D = mul(invView, normalize(make_float3(u, v, -focalLength)));
    mainR.rD = make_float3(mainR.D.x == 0.0f ? 0.0f : 1.0f / mainR.D.x,
                           mainR.D.y == 0.0f ? 0.0f : 1.0f / mainR.D.y,
                           mainR.D.z == 0.0f ? 0.0f : 1.0f / mainR.D.z);
    float t = 0.0f;

    // Standard blue
    float4 outputColor = make_float4(0.3f,//(float(x) / float(width)),
                                     0.4f,//(float(y) / float(height)),
                                     0.95f,//(float(x) / float(width)) * (float(y) / float(height)),
                                     1.0f);
    
    {
        // Full Atmospheric sky color:
        float2 skyViewRes = make_float2(200, 100);

        float nu = (((float(x) + 0.5f) / float(width)));
        float nv = (((float(y) + 0.5f) / float(height)));

        outputColor = getAtmosphericSkyView(mainR.D, mainR.O, lightDir, skyViewRes, data.envSkyViewTexture);
        outputColor = clamp4f(outputColor, 0.0f, 1.0f);
    }

    float4 cloudColor{};
    float accumulatedDensity = 0.0f;
    float lightAbsorption = 0.0f;


    // -------------------------------------------------------------------------------------
    // Code highly inspired from template IGAD version 3, IGAD/NHTV/UU - Jacco Bikker - 2006-2022
    // -------------------------------------------------------------------------------------

    // If ray is NOT in the grid
    if (!(mainR.O.x >= gridMin.x && mainR.O.x <= gridMax.x && mainR.O.y >= gridMin.y && mainR.O.y <= gridMax.y &&
          mainR.O.z >= gridMin.z && mainR.O.z <= gridMax.z))
    {
        t = intersectGrid(mainR.D, mainR.O, mainR.rD);
        //if (t > 1e33f);  // Did not intersect grid at all
    }

    if (t < 1e33f)
    {
        // Convert reversed direction into 0 or 1
        float3 stepSign =
            make_float3((mainR.D.x < 0.0f) ? 0.0f : 0.5f, (mainR.D.y < 0.0f) ? 0.0f : 0.5f, (mainR.D.z < 0.0f) ? 0.0f : 0.5f);
        //float3 stepSign =
        //    (make_float3(-copysign(1.0f, mainR.D.x), -copysign(1.0f, mainR.D.y), -copysign(1.0f, mainR.D.z)) + 1.0f) * 0.5f;
        // Step for direction with -1 meaning backwards and 1 forwards per axis.
        float3 step = 1.0f - float3(stepSign) * 2.0f;
        const float3 posInGrid = (mainR.O + (t + 0.00005f) * mainR.D);  // Position in grid
        const float3 gridPlanes = (ceil3f(posInGrid) - stepSign);  // Next boundary intersection we will intersect using ceil
        // Set starting position, making sure to clamp within grid
        float3 pos = make_float3(clampf(posInGrid.x, gridMin.x, gridMax.x - 1),
                                 clampf(posInGrid.y, gridMin.y, gridMax.y - 1),
                                 clampf(posInGrid.z, gridMin.z, gridMax.z - 1));
        // Normalized position
        float3 normPos = make_float3(pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);

        // How much to step to cross 1 full cell
        float3 tDelta = step * mainR.rD;
        float3 tMax = (gridPlanes - mainR.O) * mainR.rD;  // Max distance to cross boundary of each axis

        // Forward and backward scattering
        const float lightAngle = dot(mainR.D, lightDir);
        const float primScattering = henyenGreenstein(lightAngle, 0.8f);
        const float secScattering = henyenGreenstein(lightAngle, -0.4f);

        // How much light is reflected by each case
        const float albedoQw = 0.999f;
        const float albedoQr = 0.99f;
        const float albedoQs = 0.999f;

        // Extinction Coëfficiënt divided by Mass concentration, in m2/kg
        const float keQw = 150.0f;
        const float keQr = 3.0f;
        const float keQs = 30.0f;

        // Forward scattering g coefficient
        const float gQw = 0.85f; 
        const float gQr = 0.95f;
        const float gQs = 0.7f;


        // Start tracing through the grid
        const float3 invGridMax = make_float3(1.0f / gridMax.x, 1.0f / gridMax.y, 1.0f / gridMax.z);

        float density = 0.0f;

        float lightIntensity = 0.0f;
        float accumulatedRainDensity = 0.0f;

        float transmittance = 0.0f;

        const float nearStepSize = 0.1f;
        const float farStepSize = 0.25f;
        const float stepAdjustmentDistance = distance(make_float3(0, 0, 0), make_float3(data.sizeX, data.sizeY, data.sizeZ));
        const float startT = t;


        while (1)
        {
            if (isOutside(pos.x, pos.y, pos.z)) break;

            // Base stepsize on distance from camera
            float stepSize = nearStepSize + ((farStepSize * (t - startT)) / stepAdjustmentDistance) * data.rayRandomOffset * 25.0f;

            // Already compute distance to closest cloud
            float distanceFieldValueQw = tex3D<float>(data.SDFTextureQw, normPos.x, normPos.y, normPos.z);
            float distanceFieldValueQr = tex3D<float>(data.SDFTextureQr, normPos.x, normPos.y, normPos.z);
            float distanceFieldValueQs = tex3D<float>(data.SDFTextureQs, normPos.x, normPos.y, normPos.z);

            // Randomize stepsize
            rSeed = randomHash(unsigned(pos.x), unsigned(pos.y), unsigned(pos.z), rSeed);
            const float uniformRand = (rSeed & 0xFFFF) / 65535.0f;
            stepSize += (uniformRand - 0.5f) * data.rayRandomOffset * stepSize * 0.5f;
            // Increase stepsize for less important mixing ratios
            stepSize += distanceFieldValueQw > 1.0f ? 0.25f : 0.0f;

            // Start of Marching
            float cloudCoverage = distanceFieldValueQw < 1.0f ? calculateCloudCoverage(pos, data) : 0.0f;
            float rainCoverage = distanceFieldValueQr < 1.0f ? calculateRainCoverage(pos, data) : 0.0f;
            float snowCoverage = distanceFieldValueQs < 1.0f ? calculateSnowCoverage(pos, data) : 0.0f;

            const float airDensity = 1.0f; // TODO: to be passed as variable or use hydrostatic calculation.

            // if (x == 0 && y == 0)
            //{
            //     printf("x %i y %i, t %f, cloudCoverage %f\n", x, y, t, cloudCoverage);
            // }

            // Add noise reduction if cloud coverage is present
            const float cloudDensity = cloudCoverage > 0.0f ? calculateDensity(pos, data, cloudCoverage) : 0.0f;
            

            // Only when there is something to trace continue
            if (cloudDensity > 0.0f || rainCoverage > 0.0f || snowCoverage > 0.0f)
            {
                // Starting with calculating how much light is absorbed 
                // From the total accumulated density using exponential which correlates with Beer's law
                lightAbsorption = 1 - exp(-accumulatedDensity);

                // Calculate mass extinction coëfficient of variables 
                // By converting mixing ratio (kg/kg) to mass (kg/m3) using air density
                // Then using the extinction coëfficient, we convert to our mass extinction coëfficient (1/m)
                const float sigmaQw = cloudDensity * airDensity * keQw;
                const float sigmaQr = rainCoverage * airDensity * keQr;
                const float sigmaQs = snowCoverage * airDensity * keQs;

                // Calculate total extinction
                const float extinction = sigmaQw + sigmaQr + sigmaQs;
                // And calculate scattering based on albedos
                const float scattering = sigmaQw * albedoQw + sigmaQr * albedoQr + sigmaQs * albedoQs;

                // Density is the ray length in world size multiplied by how much the ray is consumed per meter (extinction)
                accumulatedDensity += extinction * stepSize * data.voxelSize;


                // At every step also march a ray towards the light source (sun) to check how much density is in between.
                const float lightDirMarchDensity = lightMarch(pos, lightDir, data, stepSize);
                // To gain transmittance we use exponent which corrolates with Beer's law
                const float lightTransmittance = exp(-lightDirMarchDensity);


                // Calculate multiple scattering using approximation https://www.researchgate.net/publication/262309690_Oz_the_great_and_volumetric
                const int octaves = 8;
                const float g = (sigmaQw * gQw + sigmaQr * gQr + sigmaQs * gQs) / scattering; // Mixture weighting
                float msValue = 0.0f;

                for (int n = 0; n < octaves; n++)
                {
                    const float a = powf(data.attenuation, n);  // attenuation
                    const float b = powf(data.contribution, n);  // Contribution
                    const float c = powf(data.eccentricAttenuation, n);  // Eccentricity attenuation
                    msValue += b * henyenGreenstein(lightAngle, g * c) * expf(-a * lightDirMarchDensity);
                }




                // Multiple scattering phase
                const float msVolume = fminf(extinction / 0.05f, 1.0f) * pow(lightTransmittance, 0.5f);

                // Add all scattering together
                const float directScattering = (lightTransmittance * primScattering) + (msVolume * secScattering);

                const float ambientScattering = data.ambientLightStrength * 0.01f * expf(-accumulatedDensity * 0.5f);

                const float totalLight = msValue * sunStrength + ambientScattering;


                // Calculate our final light intensity based on total light from scattering, our scattering coëfficient, 
                // reducing with more light being absorbed, and finally make sure to map it to world size.
                lightIntensity += totalLight * scattering * (1.0f - lightAbsorption) * stepSize * data.voxelSize;

                if (x == 0 && y == 0)
                {
                     printf(
                         "x %i y %i, t %f, cloudDensity %f, rainCoverage %f msVolume %f, lightTransmittance %f, "
                         "lightIntensity % f acumdDens % f lightDensity % f,directScat : % f, lightAbsorption: %f\n ",
                         x,
                         y,
                         t,
                         cloudDensity,
                         rainCoverage,
                         msVolume,
                        lightTransmittance,
                         lightIntensity,
                         accumulatedDensity,
                         lightDirMarchDensity,
                         directScattering,
                         lightAbsorption);
                 }
                if (lightAbsorption >= 1 - 0.01f)
                {
                    break;  // TODO: test out when cloud is full
                }
            }

            t += stepSize;
            pos = pos + mainR.D * stepSize;
            normPos = make_float3(pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);


            // Get value on distance field creating for faster lookup, skipping empty air
            // We loop until distance to nearest cloud becomes too small and start normal stepping again
            while (!isOutside(pos.x, pos.y, pos.z))
            {
                distanceFieldValueQw = fmaxf(tex3D<float>(data.SDFTextureQw, normPos.x, normPos.y, normPos.z) - 0.75f, 0.0f);
                distanceFieldValueQr = fmaxf(tex3D<float>(data.SDFTextureQr, normPos.x, normPos.y, normPos.z) - 0.75f, 0.0f);
                distanceFieldValueQs = fmaxf(tex3D<float>(data.SDFTextureQs, normPos.x, normPos.y, normPos.z) - 0.75f, 0.0f);
                const float closest = fminf(fminf(distanceFieldValueQw, distanceFieldValueQr), distanceFieldValueQs);

                if (closest <= 1.0f) break;

                t += fmaxf(closest - 1.0f, stepSize);
                pos = pos + mainR.D * fmaxf(closest - 1.0f, stepSize);
                normPos = make_float3(pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);
            }

            // if (tMax.x < tMax.y && tMax.x < tMax.z)  // If closest to x boundary
            //{
            //     t = tMax.x;
            //     pos.x += step.x;
            //     if (pos.x >= gridMax.x || pos.x < gridMin.x) break;
            //     tMax.x += tDelta.x;
            // }
            // else if (tMax.y < tMax.z)  // y is closer than z
            //{
            //     t = tMax.y;
            //     pos.y += step.y;
            //     if (pos.y >= gridMax.y || pos.y < gridMin.y) break;
            //     tMax.y += tDelta.y;
            // }
            // else  // z is closest
            //{
            //     t = tMax.z;
            //     pos.z += step.z;
            //     if (pos.z >= gridMax.z || pos.z < gridMin.z) break;
            //     tMax.z += tDelta.z;
            // }
        }


        //cloudColor = make_float4(lightIntensity *  lightColor.x,
        //                         lightIntensity *  lightColor.y,
        //                         lightIntensity *  lightColor.z,
        //                         1.0f);

        cloudColor = lightColor * lightIntensity;




        // toneMap

        //    float A = 0.15f;  // Shoulder strength
        //float B = 0.50f;  // Linear strength
        //float C = 0.10f;  // Linear angle
        //float D = 0.20f;  // Toe strength
        //float E = 0.02f;  // Toe numerator
        //float F = 0.30f;  // Toe denominator

        //cloudColor = ((cloudColor * (cloudColor * A + C * B) + D * E) / (cloudColor * (cloudColor * A + B) + D * F)) - E / F;

        //cloudColor.x = cloudColor.x / (cloudColor.x + 1.0f);
        //cloudColor.y = cloudColor.y / (cloudColor.y + 1.0f);
        //cloudColor.z = cloudColor.z / (cloudColor.z + 1.0f);


        //cloudColor = make_float4(cloudDensity, cloudDensity, cloudDensity, 1.0f);


    }


    //float output = tex3D<float>(data.SDFTextureQw, float(x) / float(width), float(y) / float(height), 0.5f);
    //output = clampf(output, 0.0f, 0.95f);
    //outputColor = make_float4(output, output, output, 1.0f);

    // Map density to opacity between 0 and 1
    //const float opacity = 1.0f - expf(-accumulatedDensity);

    
    const float T = expf(-accumulatedDensity);
    
    // Firs make sure the background color is scaled correctly with gamma, exposure
    float4 skycolor = make_float4(powf(outputColor.x, 2.2f), powf(outputColor.y, 2.2f), powf(outputColor.z, 2.2f), 1.0f);
    const float4 skyLinear = (skycolor / (1.0f - skycolor)) / data.exposure;

    outputColor = skyLinear * T + cloudColor;

    outputColor = make_float4(fmaxf(outputColor.x, 0.0f), fmaxf(outputColor.y, 0.0f), fmaxf(outputColor.z, 0.0f), 1.0f);

    // Exposure   

    outputColor = outputColor * data.exposure;
    outputColor = outputColor / (outputColor + 1.0f);
    outputColor =
        make_float4(powf(outputColor.x, 1.0f / 2.2f), powf(outputColor.y, 1.0f / 2.2f), powf(outputColor.z, 1.0f / 2.2f), 1.0f);

    outputColor = clamp4f(outputColor, 0.0f, 1.0f);


    //if (x == 0 && y == 0)
    //{
    //    printf(
    //        "x %i y %i, t %f, cloudColor x %f y %f z %f, outputColor  x %f y %f z %f, lightColor: x %f y %f z %f lightAbsorption %f "
    //        "\n ",
    //        x,
    //        y,
    //        t,
    //        cloudColor.x,
    //        cloudColor.y,
    //        cloudColor.z,
    //        outputColor.x,
    //        outputColor.y,
    //        outputColor.z,
    //        lightColor.x,
    //        lightColor.y,
    //        lightColor.z,
    //        lightAbsorption);
    //}

    unsigned int outputColorI = rgbaFloatToInt(outputColor);

    dOutput[x + y * width] = outputColorI;
}

__device__ float intersectGrid(float3 dir, float3 origin, float3 recDir)
{
    // test if the ray intersects the cube

    float3 gridBounds[2] = {gridMin, gridMax};

    // Code from template IGAD version 3, IGAD/NHTV/UU - Jacco Bikker - 2006-2022
    const int signx = dir.x < 0, signy = dir.y < 0, signz = dir.z < 0;
    float tmin = (gridBounds[signx].x - origin.x) * recDir.x;
    float tmax = (gridBounds[1 - signx].x - origin.x) * recDir.x;
    const float tymin = (gridBounds[signy].y - origin.y) * recDir.y;
    const float tymax = (gridBounds[1 - signy].y - origin.y) * recDir.y;
    if (tmin > tymax || tymin > tmax) return 1e34f;
    tmin = fmaxf(tmin, tymin), tmax = fminf(tmax, tymax);
    const float tzmin = (gridBounds[signz].z - origin.z) * recDir.z;
    const float tzmax = (gridBounds[1 - signz].z - origin.z) * recDir.z;
    if (tmin > tzmax || tzmin > tmax) return 1e34f;
    if ((tmin = fmaxf(tmin, tzmin)) > 0) return tmin;

    return 1e34f;
}





__device__ float calculateCloudCoverage(float3& pos, environmentData& data)
{
    float output = 0.0f;

    const float QW = tex3D<float>(data.QwTexture, pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);
    output = QW;

    //

    if (QW > data.minQw)
    {
        // Min and Max in terms of scud, values above will be more obvious densities


        //output = clampf(powf(((log(QW * 100.0f) + 3) / 3.0f), 0.2f), 0.0f, 1.0f);
        
        //output = clampf((log10f(QW) + data.noiseCutoffValue) * data.noisePlateauValue, scudValue, 0.99f);
        //// Values between 0.00001 and 0.001 will be mapped 0 to 1
        
        //output = clampf((logf(QW) - logf(data.minQw)) / (logf(data.maxQw) - logf(data.minQw)), 0.0f, 0.9f);

        //output = clampf(3.825f + 0.39f * log(QW), 0.0f, 0.99f);



        //if (blockIdx.x * blockDim.x + threadIdx.x == 0 && blockIdx.y * blockDim.y + threadIdx.y == 0)
        //{
        //    printf("pos: x %f, y %f, z %f, sizeX %i, sizeY %i, sizeZ %i, Cloud found: %f\n",
        //           pos.x,
        //           pos.y,
        //           pos.z,
        //           data.sizeX,
        //           data.sizeY,
        //           data.sizeZ,
        //           QW);
        //}
    }

    return output;
}

__device__ float calculateRainCoverage(float3& pos, environmentData& data) 
{ 
    float output = 0.0f;

    const float QR = tex3D<float>(data.QrTexture, pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);
    output = QR;
    if (QR > QRMIN)
    {
        // Min and Max, values above will be more obvious densities
        //output = clampf((logf(QR) - logf(QRMIN)) / (logf(QRMAX) - logf(QRMIN)), 0.0f, 0.999f);
    }

    return output;//*0.0025f;
}

__device__ float calculateSnowCoverage(float3& pos, environmentData& data)
{
    float output = 0.0f;

    const float QS = tex3D<float>(data.QsTexture, pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);
    output = QS;
    if (QS > QSMIN)
    {
        // Min and Max, values above will be more obvious densities
        //output = clampf((logf(QS) - logf(QSMIN)) / (logf(QSMAX) - logf(QSMIN)), 0.0f, 0.999f);
    }

    return output;
}

__device__ float calculateDensity(float3& pos,
                                  environmentData& data,
                                  const float cloudCoverage)
{
    float noise = 0.0f;

    // Expensive texture lookup (higher resolution = more expensive)
    // This resolution increase increases our coordinate and since our texture repeats, we essentially increase the amount
    // of textures
    const float maxGrid = 1.0f / fmaxf(gridMax.x, fmaxf(gridMax.y, gridMax.z));
    float3 normPos = pos * maxGrid;

    // Offset from velocity
    const float velX = tex3D<float>(data.velXTexture, normPos.x, normPos.y, normPos.z) / data.voxelSize;
    const float velY = tex3D<float>(data.velYTexture, normPos.x, normPos.y, normPos.z) / data.voxelSize;
    const float velZ = tex3D<float>(data.velZTexture, normPos.x, normPos.y, normPos.z) / data.voxelSize;


    const float resolutionIncrease = 16.0f;
    noise = tex3D<float>(data.noiseTexture,
                         pos.x * maxGrid * resolutionIncrease + velX,
                         pos.y * maxGrid * resolutionIncrease + velY,
                         pos.z * maxGrid * resolutionIncrease + velZ);

    const float coverage = clampf((logf(cloudCoverage) - logf(data.minQw)) / (logf(data.maxQw) - logf(data.minQw)), 0.0f, 1.0f);


    const float edgeFactor = 1.0f - coverage;
    const float eLo = 0.5f * edgeFactor * data.noiseReduction;
    // Calculate erosion based on coverage, remapped with the noise, edgefactor and variable reduction
    float eroded = remap(coverage, noise * edgeFactor * data.noiseReduction, 1.0f, 0.0f, 1.0f);
    float fullEroded = remap(1.0f, noise * edgeFactor * data.noiseReduction, 1.0f, 0.0f, 1.0f);

    // Normalize with full coverage
    eroded = clampf((eroded) / fmaxf(fullEroded, 1e-3f), 0.0f, 1.0f);
    const float erodedMean = clampf((coverage - eLo) / fmaxf(1.0f - eLo, 1e-3f), 1e-2f, 1.0f);

    const float boost = fminf(eroded / erodedMean, 4.0f);

    //float value = clampf(pow(noise - (1.0f - cloudCoverage), data.noiseReduction), 0.0f, 1.0f);
    //eroded = eroded >= data.noiseCutoffValue ? 1.0f : 0.0f; // Cut off at value and set remaining to 1
    return fmaxf(coverage - noise * edgeFactor * data.noiseReduction, 0.0f) * cloudCoverage;
}

__device__ float lightMarch(float3 pos, const float3& lightDir, environmentData& data, float stepSize)
{
    // TODO: This can be precomputed every simulation update and put in texture

    // Trace from position until we are out of the grid
    unsigned int rSeed = 128;
    float density = 0.0f;
    float lightStepSize = 0.0f;
    float3 normPos = make_float3(pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);
    float distanceFieldValueQw = 0.0f;
    float distanceFieldValueQs = 0.0f;
    float prevDFieldValueQw = 0.0f;
    //float distanceFieldValueQr = 0.0f;

    float t = 0.0f;
    int samples = 0;
    const int maxSamples = 25;
    float standardStepSize = stepSize;

    const float airDens = 1.0f; // TODO: use variable or calculate

    // How much light is reflected by each case
    const float albedoQw = 0.999f;
    const float albedoQr = 0.99f;
    const float albedoQs = 0.999f;

    // Extinction Coëfficiënt divided by Mass concentration, in m2/kg
    const float keQw = 150.0f;
    const float keQr = 3.0f;
    const float keQs = 30.0f;

    while (1)
    {
        // Stepsize
        lightStepSize = standardStepSize;
        rSeed = randomHash(unsigned(pos.x), unsigned(pos.y), unsigned(pos.z), rSeed);
        const float uniformRand = (rSeed & 0xFFFF) / 65535.0f;
        lightStepSize += (uniformRand - 0.5f) * data.rayRandomOffset * lightStepSize * 0.5f;


        const float cloudCoverage = calculateCloudCoverage(pos, data);
        //float rainCoverage = calculateRainCoverage(pos, data); Ignoring rain for now since it does not add much and is much slower
        float snowCoverage = calculateSnowCoverage(pos, data);

        // Maybe if we want to erode, but that seems not needed and unnecessary expensive
        const float cloudDens = cloudCoverage > 0.0f ? calculateDensity(pos, data, cloudCoverage) : 0.0f;


        // Calculate mass extinction coëfficient of variables
        // By converting mixing ratio (kg/kg) to mass (kg/m3) using air density
        // Then using the extinction coëfficient, we convert to our mass extinction coëfficient (1/m)
        const float sigmaQw = cloudDens * airDens * keQw;
        const float sigmaQr = 0.0f;//rainCoverage * airDens * keQr;
        const float sigmaQs = snowCoverage * airDens * keQs;

        // Calculate total extinction
        const float extinction = sigmaQw + sigmaQr + sigmaQs;

        // Density is the ray length in world size multiplied by how much the ray is consumed per meter (extinction)
        density += extinction * stepSize * data.voxelSize;

        samples++;

        if (samples >= maxSamples || density >= data.multipleScatteringDepthPower) break;  // Full opacity, dont need to trace anymore


        //pos = pos + lightDir * lightStepSize;
        //normPos = make_float3(pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);
        //t += lightStepSize;



        // Get value on distance field creating for faster lookup, skipping empty air
        // We loop until distance to nearest cloud becomes too small and start normal stepping again
        distanceFieldValueQw = 0.0f;
        distanceFieldValueQs = 0.0f;
        while (!isOutside(pos.x, pos.y, pos.z))
        {
            // Push position forwards into the light direction
            float distance = fmaxf(fminf(distanceFieldValueQw, distanceFieldValueQs), lightStepSize);
            t += distance;
            pos = pos + lightDir * distance;
            normPos = make_float3(pos.x / gridMax.x, pos.y / gridMax.y, pos.z / gridMax.z);

            // Get Distance Field Value to check if we are out of the cloud and how far away we can move freely
            distanceFieldValueQw = fmaxf(tex3D<float>(data.SDFTextureQw, normPos.x, normPos.y, normPos.z) - 0.75f, 0.0f);
            distanceFieldValueQs = fmaxf(tex3D<float>(data.SDFTextureQs, normPos.x, normPos.y, normPos.z) - 0.75f, 0.0f);

            // Increase stepsize if no cloud is nearby
            standardStepSize = distanceFieldValueQw < 0.5f ? stepSize + 0.25f : stepSize;


            //                    if (blockIdx.x * blockDim.x + threadIdx.x == 0 && blockIdx.y * blockDim.y + threadIdx.y == 0)
            //{
            //    printf(
            //        "x %f y %f z %f, t %f, cloudCoverage %f, cloudDens %f, density %f, DFQw %f, DFQr %f, samples: %i, "
            //        "standardStepSize %f, stepsize: %f\n ",
            //        pos.x,
            //        pos.y,
            //        pos.z,
            //        t,
            //        cloudCoverage,
            //        cloudDens,
            //        density,
            //        distanceFieldValueQw,
            //        0.0f,
            //        samples,
            //        standardStepSize,
            //        stepSize);
            //}

            // If going out of the cloud in the next step
            //if (prevDFieldValueQw == 0.0f && distanceFieldValueQw > 0.0f)
            //{
            //    // Check if we already changed step size
            //    if (standardStepSize == stepSize)
            //    {
            //        // Go back the distance
            //        pos = pos - lightDir * distance;
            //        t -= distance;

            //        // Set new stepsize smaller to gain extra details
            //        const float extraSampleAmount = 4.0f;
            //        standardStepSize = fabsf(distance - distanceFieldValueQw) / extraSampleAmount;
            //        prevDFieldValueQw = distanceFieldValueQw;

            //        // Move ahead this new small distance
            //        pos = pos + lightDir * standardStepSize;
            //        t += standardStepSize;
            //        distanceFieldValueQw = 0.0f;
            //        break;
            //    }
            //    else
            //    {
            //        standardStepSize = stepSize;
            //    }
            //}

            prevDFieldValueQw = distanceFieldValueQw;



            // If Distance Field Value is smaller than our stepsize, we go back to calculating
            // distanceFieldValueQr = tex3D<float>(data.SDFTextureQr, normPos.x, normPos.y, normPos.z);
            // const float closest = fminf(distanceFieldValueQw, distanceFieldValueQr);
            if (distanceFieldValueQw <= lightStepSize || distanceFieldValueQs <= lightStepSize) break;
        }

        if (isOutside(pos.x, pos.y, pos.z)) break;
    }

    return density;
}

__device__ float henyenGreenstein(float inCosAngle, float inG) 
{ 
    float num = 1.0f - inG * inG;
    float denom = 1.0f + inG * inG - 2.0f * inG * inCosAngle;
    float rsqrtDenom = 1.0f / sqrt(denom);
    return num * rsqrtDenom * rsqrtDenom * rsqrtDenom * (1.0f / (4.0f * 3.14159265359));
}
