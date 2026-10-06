#pragma once

#include "config.h"

#include <cuda_runtime.h> 
#include <iostream>

// CUDA constants (declared in environment.cu)
extern __constant__ int simSizeX;
extern __constant__ int simSizeY;
extern __constant__ int simSizeZ;
extern __constant__ int simSize;
extern __constant__ float voxelSize;
extern __constant__ int simSizeGround;
// Speed, dt and spread of blocks in depth used in kernels
extern __constant__ float simSpeed;
extern __constant__ float simDeltaTime;
extern __constant__ float invBlockSpreadDepth;

// Both branch for host and device due to difference in variable location
__host__ __device__ inline int getIdx(int x, int y, int z)
{
#ifdef __CUDA_ARCH__
	return x + y * simSizeX + z * simSizeX * simSizeY;
#else
	return x + y * GRIDSIZESKYX + z * GRIDSIZESKYX * GRIDSIZESKYY;
#endif
}



__host__ __device__ inline void getCoord(const int index, int& x, int& y, int& z)
{
#ifdef __CUDA_ARCH__
	// If invalid
	if (index < 0 || index > simSize)
	{
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	const int xy = index % (simSizeX * simSizeY);
	x = xy % simSizeX;
	y = (xy - x) / simSizeX;
	z = (index - xy) / (simSizeX * simSizeY);
#else
	// If invalid
	if (index < 0 || index > GRIDSIZESKY)
	{
		x = 0;
		y = 0;
		z = 0;
		return;
	}
	const int xy = index % (GRIDSIZESKYX * GRIDSIZESKYY);
	x = xy % GRIDSIZESKYX;
	y = (xy - x) / GRIDSIZESKYX;
	z = (index - xy) / (GRIDSIZESKYX * GRIDSIZESKYY);
#endif
}

__host__ __device__ inline bool isOutside(int x, int y, int z)
{
#ifdef __CUDA_ARCH__
	if (x + 1 > simSizeX) return true;
	else if (x < 0) return true;
	if (y + 1 > simSizeY) return true;
	else if (y < 0) return true;
	if (z + 1 > simSizeZ) return true;
	else if (z < 0) return true;
	return false;
#else
	if (x + 1 > GRIDSIZESKYX) return true;
	else if (x < 0) return true;
	if (y + 1 > GRIDSIZESKYY) return true;
	else if (y < 0) return true;
	if (z + 1 > GRIDSIZESKYZ) return true;
	else if (z < 0) return true;
	return false;
#endif
}

__host__ __device__ inline bool isOutside(int idx)
{
#ifdef __CUDA_ARCH__
	if (idx + 1 > simSize || idx < 0) return true;
	return false;
#else
	if (idx + 1 > GRIDSIZESKY || idx < 0) return true;
	return false;
#endif
}

// Retrieve grid and block dimensions based on current hardware
__host__ inline void getGridBlockDims(dim3& _gridDim, dim3& _blockDim)
{
    // Set block and grid dimension based on GPU specs.
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    unsigned int totalThreadsPerBlock = prop.maxThreadsPerBlock / 4;  // Use smaller threads so we have more leeway
    unsigned int totalBlocksPerGrid = prop.maxBlocksPerMultiProcessor * prop.multiProcessorCount;
    // unsigned int totalRegsPerThread = prop.regsPerBlock / totalThreadsPerBlock;
    //  Set block dimensions to be square root of the max threads available. Making use of as much threads as possible
    const unsigned int threadsBlock = uint32_t(floor(sqrt(totalThreadsPerBlock)));
    // If one side is smaller than 1 block, we still want to use as many threads as possible per block
    if (totalThreadsPerBlock > unsigned(GRIDSIZESKYX))
    {
        _blockDim.x = GRIDSIZESKYX;
        _blockDim.y = std::min(uint32_t(GRIDSIZESKYY), uint32_t(double(totalThreadsPerBlock) / double(_blockDim.x)));
    }
    // else if (threadsBlock > GRIDSIZESKYY)
    //{
    //	_blockDim.y = GRIDSIZESKYY;
    //	_blockDim.x = std::min(uint32_t(GRIDSIZESKYX), uint32_t(double(totalThreadsPerBlock) / double(_blockDim.y)));
    // }
    else
    {
        _blockDim.x = std::min(uint32_t(GRIDSIZESKYX), threadsBlock);
        _blockDim.y = std::min(uint32_t(GRIDSIZESKYY), threadsBlock);
    }

    // Amount of blocks on the x and y axis is determined by how big our grid is
    _gridDim.x = unsigned int(ceil(double(GRIDSIZESKYX) / double(_blockDim.x)));
    _gridDim.y = unsigned int(ceil(double(GRIDSIZESKYY) / double(_blockDim.y)));
    // Error Check
    if (_gridDim.x * _gridDim.y >= totalBlocksPerGrid)
    {
        printf(
            "Error: current simulation size is greater than available theads and blocks on the x and y axis, use a small "
            "simulation size!\n");
        return;
    }
    // Depth is determined by how many blocks we have in total and use per z slice.
    _gridDim.z =
        unsigned int(std::min(double(GRIDSIZESKYZ), floor(double(totalBlocksPerGrid) / double(_gridDim.x * _gridDim.y))));
}
