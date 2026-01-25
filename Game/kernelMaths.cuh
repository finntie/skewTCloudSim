#pragma once

#include <cuda_runtime.h>

__global__ void setToValue(float* array, const float value, const int width);

__global__ void multiplyValues(float* array1, const float* array2, const int width);

__global__ void divideValues(float* array1, const float* array2, const int width);

__global__ void subtractValue(float* array, const float value, const int width);

__global__ void debugPrintArray(const float* array);

__global__ void debugPrintArray(const int* array);