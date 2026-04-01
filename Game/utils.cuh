#pragma once

#include "config.h"

#include <cuda_runtime.h> 


__host__ __device__ inline int getIdx(int x, int y, int z)
{
	return x + y * GRIDSIZESKYX + z * GRIDSIZESKYX * GRIDSIZESKYY;
}

__host__ __device__ inline void getCoord(const int index, int& x, int& y, int& z)
{
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
}

__host__ __device__ inline bool isOutside(int x, int y, int z)
{
	if (x + 1 > GRIDSIZESKYX) return true;
	else if (x < 0) return true;
	if (y + 1 > GRIDSIZESKYY) return true;
	else if (y < 0) return true;
	if (z + 1 > GRIDSIZESKYZ) return true;
	else if (z < 0) return true;
	return false;
}

__host__ __device__ inline bool isOutside(int idx)
{
	if (idx + 1 > GRIDSIZESKY || idx < 0) return true;
	return false;
}