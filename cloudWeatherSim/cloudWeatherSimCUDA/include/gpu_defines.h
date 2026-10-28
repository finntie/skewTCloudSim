#pragma once

// Makes sure all GPU definitions are valid on the CPU

#if defined(__CUDACC__) || defined(__HIPCC__) || defined(__HIP__)
#define GPU_COMPILER 1
#endif

#ifndef GPU_COMPILER
#define __host__
#define __device__
#define __global__
#define __constant__
#define __shared__
#define __forceinline__ inline
#endif
