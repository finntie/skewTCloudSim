#include "kernelGround.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "environment.h"
#include "meteoconstants.cuh"
#include "meteoformulas.cuh"

//__constant__ float GHeightGPU[GRIDSIZEGROUND];
//__constant__ float defaultVel[GRIDSIZESKYY];


__global__ void initGroundHeightGPU(int* _GHeight, const float* noise, const float maxHeight)
{
	int tX = threadIdx.x;
	_GHeight[tX] = static_cast<int>(roundf(noise[tX] * maxHeight));
}

__global__ void resetValueInGround(float* array, const int* _GHeight)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (tY > _GHeight[tX]) return;

	array[idx] = 0.0f;
}

__global__ void computeIsenTempGroundGPU(float* array, const float* isenTemp, const float* groundPressure, const float* pressures, const int* _GHeight)
{
	const int tX = threadIdx.x;

	const int GH = _GHeight[tX] + 1 >= GRIDSIZESKYY ? GRIDSIZESKYY - 1 : _GHeight[tX] + 1;
	const float T = potentialTempGPU(isenTemp[GH - 1] - 273.15f, groundPressure[tX], pressures[GH]);
	array[tX] = T + 273.15f;
}

__global__ void getFirstValidIndex(int* firstValid, const int* _GHeight)
{
	for (int x = 0; x < GRIDSIZESKYX; x++)
	{
		if (_GHeight[x] >= GRIDSIZESKYY - 2) continue;
		*firstValid = x + (_GHeight[x] + 1) * GRIDSIZESKYX;
	}
}

