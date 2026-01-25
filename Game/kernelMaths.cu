#include "kernelMaths.cuh"

#include <CUDA/cmath>
#include <cuda.h>

#include <stdio.h>

//TDO add .cu implementations for these.
__global__ void setToValue(float* array, const float value, const int width)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * width;

    array[idx] = value;
}

__global__ void multiplyValues(float* array1, const float* array2, const int width)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * width;

    array1[idx] *= array2[idx];
}

__global__ void divideValues(float* array1, const float* array2, const int width)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * width;
    array1[idx] /= array2[idx];
}

__global__ void subtractValue(float* array, const float value, const int width)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * width;

    array[idx] -= value;
}

__global__ void debugPrintArray(const float* array)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * blockDim.x;

    printf("DebugPrintArray (%i, %i) = array[%i]: %f\n", tX, tY, idx, array[idx]);
}

__global__ void debugPrintArray(const int* array)
{
    const int tX = threadIdx.x;
    const int tY = blockIdx.x;
    const int idx = tX + tY * blockDim.x;

    printf("DebugPrintArray (%i, %i) = array[%i]: %i\n", tX, tY, idx, array[idx]);
}