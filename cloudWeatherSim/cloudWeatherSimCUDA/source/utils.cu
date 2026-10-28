#include "utils.cuh"

#include "msvc_defines.h"
#include <hip/hip_runtime.h>

// Cu file needed for the hip implementation.

// Retrieve grid and block dimensions based on current hardware
__host__ void getGridBlockDims(dim3& _gridDim, dim3& _blockDim)
{
    // Set block and grid dimension based on GPU specs.
    hipDeviceProp_t prop;
    hipGetDeviceProperties(&prop, 0);
    unsigned int totalThreadsPerBlock = prop.maxThreadsPerBlock / 4;  // Use smaller threads so we have more leeway
    unsigned int totalBlocksPerGrid = prop.maxBlocksPerMultiProcessor * prop.multiProcessorCount;
    // unsigned int totalRegsPerThread = prop.regsPerBlock / totalThreadsPerBlock;
    //  Set block dimensions to be square root of the max threads available. Making use of as much threads as possible
    const unsigned int threadsBlock = uint32_t(floor(sqrt(totalThreadsPerBlock)));
    // If one side is smaller than 1 block, we still want to use as many threads as possible per block
    if (totalThreadsPerBlock > unsigned(GRIDSIZESKYX))
    {
        _blockDim.x = GRIDSIZESKYX;
        _blockDim.y = std::min(uint32_t(GRIDSIZESKYY), uint32_t(double(totalThreadsPerBlock) / double(_blockDim.x)));
    }
    // else if (threadsBlock > GRIDSIZESKYY)
    //{
    //	_blockDim.y = GRIDSIZESKYY;
    //	_blockDim.x = std::min(uint32_t(GRIDSIZESKYX), uint32_t(double(totalThreadsPerBlock) / double(_blockDim.y)));
    // }
    else
    {
        _blockDim.x = std::min(uint32_t(GRIDSIZESKYX), threadsBlock);
        _blockDim.y = std::min(uint32_t(GRIDSIZESKYY), threadsBlock);
    }

    // Amount of blocks on the x and y axis is determined by how big our grid is
    _gridDim.x = unsigned(ceil(double(GRIDSIZESKYX) / double(_blockDim.x)));
    _gridDim.y = unsigned(ceil(double(GRIDSIZESKYY) / double(_blockDim.y)));
    // Error Check
    if (_gridDim.x * _gridDim.y >= totalBlocksPerGrid)
    {
        printf(
            "Error: current simulation size is greater than available theads and blocks on the x and y axis, use a small "
            "simulation size!\n");
        return;
    }
    // Depth is determined by how many blocks we have in total and use per z slice.
    _gridDim.z =
        unsigned(std::min(double(GRIDSIZESKYZ), floor(double(totalBlocksPerGrid) / double(_gridDim.x * _gridDim.y))));
}