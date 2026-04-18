#include "kernelGround.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "utils.cuh"
//#include "meteoconstants.cuh"
#include "meteoformulas.cuh"


__global__ void initGroundHeightGPU(int* _GHeight, const float* noise, const float maxHeight)
{
	const int x = threadIdx.x;
	const int z = blockIdx.x;
	const int idxG = x + z * GRIDSIZESKYX;
	_GHeight[idxG] = static_cast<int>(roundf(noise[idxG] * maxHeight));
}

__global__ void resetValueInGround(float* array, const int* _GHeight)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;
	
	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		const int idx = getIdx(x, y, z);
		if (y > _GHeight[x + z * GRIDSIZESKYX])
		{
			continue;
		}
		array[idx] = 0.0f;
	}
}

__global__ void computeIsenTempGroundGPU(float* array, const float* isenTemp, const float* groundPressure, const float* pressures, const int* _GHeight)
{
	const int tX = threadIdx.x;
	const int tZ = blockIdx.x;
	const int idx = tX + tZ * GRIDSIZESKYX;
	
	const int GH = _GHeight[idx] + 1 >= GRIDSIZESKYY ? GRIDSIZESKYY - 1 : _GHeight[idx] + 1;
	const float T = potentialTempGPU(isenTemp[GH - 1] - 273.15f, groundPressure[idx], pressures[getIdx(tX, GH, tZ)]);
	array[idx] = T + 273.15f;
}

//Deprecated
//__global__ void getFirstValidIndex(int* firstValid, const int* _GHeight)
//{
//	for (int x = 0; x < GRIDSIZESKYX; x++)
//	{
//		if (_GHeight[x] >= GRIDSIZESKYY - 2) continue;
//		*firstValid = x + (_GHeight[x] + 1) * GRIDSIZESKYX;
//	}
//}

