#include "math/cudaMath.cuh"

#include "msvc_defines.h"
#include <hip/hip_runtime.h>

__constant__ float invBlockSpreadDepth{ 1.0f };

__device__ unsigned long randomState = 1;

void setConstantsCudaMath(float _invBlockSpreadDepth, void* stream)
{
    if (_invBlockSpreadDepth > 0.0f) hipMemcpyToSymbolAsync(HIP_SYMBOL(invBlockSpreadDepth), &_invBlockSpreadDepth, sizeof(float), 0, hipMemcpyHostToDevice, static_cast<hipStream_t>(stream));
    hipError_t err = hipGetLastError();
    if (err != hipSuccess) {
        std::cerr << "error1: " << hipGetErrorString(err) << std::endl;
    }
}

__global__ void setToValue(int3 size, float* array, const float value, const int depth, const int offset)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        array[idx] = value;
    }
}

__global__ void testkernel()
{
    // Does nothing
}

__global__ void multiplyValues(int3 size, float* array1, const float* array2, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        array1[idx] *= array2[idx];
    }
}

__global__ void divideValues(int3 size, float* array1, const float* array2, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        array1[idx] /= array2[idx];
    }
}

__global__ void divideValuesFull(int3 size, float* array1, const float* array2)
{
    int x = threadIdx.x + blockDim.x * blockIdx.x;
    int y = threadIdx.y + blockDim.y * blockIdx.y;
    int z = int(ceilf(float(blockIdx.z) * invBlockSpreadDepth));  // Get z index from spread and block index on z dimension.

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (; z < fminf(size.z, ceilf(float(blockIdx.z + 1) * invBlockSpreadDepth)); z++)
    {
        int idx = x + y * size.x + z * size.x * size.y;
        array1[idx] /= array2[idx];
    }
}

__global__ void subtractValue(int3 size, float* array, const float value, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        // if (z == 16 && x == 16)printf("x %i, y %i, z %i, value = %f\n", x, y, z, array[idx]);
        array[idx] -= value;
        // if (z == 16 && x == 16)printf("x %i, y %i, z %i, value2 = %f\n", x, y, z, array[idx]);
    }
}

__global__ void subtractArrayFull(int3 size, float* array1, const float* array2)
{
    int x = threadIdx.x + blockDim.x * blockIdx.x;
    int y = threadIdx.y + blockDim.y * blockIdx.y;
    int z = int(ceilf(float(blockIdx.z) * invBlockSpreadDepth));  // Get z index from spread and block index on z dimension.

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (; z < fminf(size.z, ceilf(float(blockIdx.z + 1) * invBlockSpreadDepth)); z++)
    {
        int idx = x + y * size.x + z * size.x * size.y;
        array1[idx] -= array2[idx];
    }
}

__global__ void debugPrintArray(int3 size, const float* array, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        printf("DebugPrintArray (X: %i, Y: %i, Z: %i) = array[%i]: %e\n", x, y, z, idx, array[idx]);
    }
}

__global__ void debugPrintArray(int3 size, const int* array, const int depth)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        printf("DebugPrintArray (X: %i, Y: %i, Z: %i) = array[%i]: %i\n", x, y, z, idx, array[idx]);
    }
}

__global__ void randomArray(int3 size, float* array, const float min, const float max, const int depth, const unsigned int seed)
{
    const int x = threadIdx.x;
    const int y = blockIdx.x;
    int z = 0;

    if (x >= size.x || y >= size.y || z >= size.z) return;

    if (x == 0 && y == 0 && seed != 0) randomState = seed;

    const int precision = 1000;

    for (z = 0; z < depth; z++)
    {
        const int idx = x + y * blockDim.x + z * blockDim.x * gridDim.x;
        const float rand = float(randomCuda(randomState + idx) % precision) / float(precision);
        array[idx] += rand * (max - min) + min;
    }
}