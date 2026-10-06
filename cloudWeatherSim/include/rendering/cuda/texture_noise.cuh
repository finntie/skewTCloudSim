#pragma once

#include <cuda_runtime.h>


/// <summary Generate a combination of perlin and worley noise, threads on x, y and z should match resolution size</summary>
/// <param name="output">Output data assuming the size of resolution^3</param>
/// <param name="resolution">How many pixels in the 3D texture? Higher resolution makes texture more sharp.</param>
/// <param name="octaves">How many octaves for the perlin noise, more octaves means more detail, max being 16. </param>
/// <param name="samplePoints">How many Sample Points for the texture per axis (should be smaller than resolution), more points meaning
/// smaller/more detailed 'clouds'</param> <returns></returns>
__global__ void generateWorley(float* output,
    int resolution,
    int samplePoints,
    const float contribution = 1.0f,
    unsigned long long seed = 1);

__global__ void combineWithPerlin(float* output, const int resolution, const int seed);

// code from https://www.shadertoy.com/view/4fX3D8 

/// <summary>/// /// </summary>
/// <param name="output">Output array of float values between 0 and 1</param>
/// <param name="resolution">Resolution of the texture</param>
/// <param name="gridSize">Size of the noise cells (1 - inf)</param>
/// <param name="seed">Seed for randomizing</param>
/// <param name="octaves">Number of layer noises, 1 - inf, not much improvement after 6</param>
/// <param name="lacunarity">Scaling of successive octaves (1 - inf)</param>
/// <param name="presistence">Amplitude reduction (0 - 1) </param>
void __global__ alligatorNoise(float* output, int resolution, int gridSize, int seed, const int octaves, const float lacunarity, const float presistence);