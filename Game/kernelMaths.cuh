#pragma once

#include <cuda_runtime.h>

// Sets array to value, use for how many blocks in Y and how many threads in X.
__global__ void setToValue(float* array, const float value, const int depth, const int offset = 0);

// Multiplies array2 and array1 together into array1, use for how many blocks in Y and how many threads in X.
__global__ void multiplyValues(float* array1, const float* array2, const int depth);

// Divides array1 with array2 into array1, use for how many blocks in Y and how many threads in X.
__global__ void divideValues(float* array1, const float* array2, const int depth);

// Subtracts value from array, use for how many blocks in Y and how many threads in X.
__global__ void subtractValue(float* array, const float value, const int depth);

// Debug print float array, use for how many blocks in Y and how many threads in X.
__global__ void debugPrintArray(const float* array, const int depth);

// Debug print int array, use for how many blocks in Y and how many threads in X.
__global__ void debugPrintArray(const int* array, const int depth);