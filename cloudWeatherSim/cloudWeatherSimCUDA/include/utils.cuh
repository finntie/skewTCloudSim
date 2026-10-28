#pragma once

#include "config.hpp"

#include <iostream>
#include <mutex>

#ifdef __CUDACC__
#include "msvc_defines.h"
#include <hip/hip_runtime.h>
#else
#include "gpu_defines.h"
#endif


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

// Global mutex to lock parts of code for threading
// Recursive mutex for possible multiple locks
inline std::recursive_mutex mtx;

inline void lockGlobal() { mtx.lock(); }

inline void unlockGlobal() { mtx.unlock(); }   

struct dim3;

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

    int sx = 0, sy = 0, total = 0;
#ifdef __CUDA_ARCH__ 
    sx = simSizeX, sy = simSizeY, total = simSize;
#else
    sx = GRIDSIZESKYX, sy = GRIDSIZESKYY, total = GRIDSIZESKY;
#endif

    // If invalid
    if (index < 0 || index >= total)
    {
        x = 0;
        y = 0;
        z = 0;
        return;
    }
    const int xy = index % (sx * sy);
    x = xy % sx;
    y = (xy - x) / sx;
    z = (index - xy) / (sx * sy);
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
__host__ void getGridBlockDims(dim3& _gridDim, dim3& _blockDim);
