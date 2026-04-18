#pragma once

#include <cuda_runtime.h> 


//-------------------Other------------------

__global__ void initGroundHeightGPU(int* _GHeight, const float* noise, const float maxHeight);
__global__ void resetValueInGround(float* array, const int* _GHeight);
__global__ void computeIsenTempGroundGPU(float* array, const float* isenTemp, const float* groundPressure, const float* pressures, const int* _GHeight);
//__global__ void getFirstValidIndex(int* firstValid, const int* _GHeight);
//------------------------------------------
