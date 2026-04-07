#include "kernelMaths.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

#include <random>
#include <stdio.h>

__device__ unsigned long randomState = 1;

__global__ void setToValue(float* array, const float value, const int depth, const int offset)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        array[idx] = value;
    }
}

__global__ void multiplyValues(float* array1, const float* array2, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        array1[idx] *= array2[idx];
    }
}

__global__ void divideValues(float* array1, const float* array2, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        array1[idx] /= array2[idx];
    }
}

__global__ void subtractValue(float* array, const float value, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
       // if (z == 16 && x == 16)printf("x %i, y %i, z %i, value = %f\n", x, y, z, array[idx]);
        array[idx] -= value;
        //if (z == 16 && x == 16)printf("x %i, y %i, z %i, value2 = %f\n", x, y, z, array[idx]);

    }
}

__global__ void debugPrintArray(const float* array, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        printf("DebugPrintArray (X: %i, Y: %i, Z: %i) = array[%i]: %e\n", x, y, z, idx, array[idx]);
    }
}

__global__ void debugPrintArray(const int* array, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        printf("DebugPrintArray (X: %i, Y: %i, Z: %i) = array[%i]: %i\n", x, y, z, idx, array[idx]);
    }
}

__device__ unsigned int randomCuda(unsigned int seed)
{
    unsigned int s = seed * 747796405u + 2891336453u;
    unsigned int w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}

__global__ void randomArray(float* array, const float min, const float max, const int depth, const unsigned int seed)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x == 0 && y == 0 && seed != 0) randomState = seed;

    const int precision = 1000;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        const float rand = float(randomCuda(randomState + idx) % precision) / float(precision);
        array[idx] += rand * (max - min) + min;
    }
}
