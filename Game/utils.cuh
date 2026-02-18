#pragma once

#include "config.h"

#include <cuda_runtime.h> 


__host__ __device__ inline int getIdx(int x, int y, int z)
{
	return x + y * GRIDSIZESKYX + z * GRIDSIZESKYX * GRIDSIZESKYY;
}

__host__ __device__ inline bool isOutside(int x, int y, int z)
{
	if (x > GRIDSIZESKYX) return true;
	else if (x < 0) return true;
	if (y > GRIDSIZESKYY) return true;
	else if (y < 0) return true;
	if (z > GRIDSIZESKYZ) return true;
	else if (z < 0) return true;
	return false;
}