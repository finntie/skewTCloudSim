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

// Global mutex to lock parts of code for threading
// Recursive mutex for possible multiple locks
inline std::recursive_mutex mtx;

inline void lockGlobal() { mtx.lock(); }

inline void unlockGlobal() { mtx.unlock(); }   

struct dim3;

// Both branch for host and device due to difference in variable location
__host__ __device__ inline int getIdx(simParams p, int x, int y, int z)
{
	return x + y * p.sizeX + z * p.sizeX * p.sizeY;
}



__host__ __device__ inline void getCoord(simParams p, const int index, int& x, int& y, int& z)
{
    int sx = p.sizeX, sy = p.sizeY, total = p.size;

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

__host__ __device__ inline bool isOutside(simParams p, int x, int y, int z)
{
	if (x + 1 > p.sizeX) return true;
	else if (x < 0) return true;
	if (y + 1 > p.sizeY) return true;
	else if (y < 0) return true;
	if (z + 1 > p.sizeZ) return true;
	else if (z < 0) return true;
	return false;
}

__host__ __device__ inline bool isOutside(simParams p, int idx)
{
	if (idx + 1 > p.size || idx < 0) return true;
	return false;
}

// Retrieve grid and block dimensions based on current hardware
__host__ void getGridBlockDims(simParams p, dim3& _gridDim, dim3& _blockDim);
