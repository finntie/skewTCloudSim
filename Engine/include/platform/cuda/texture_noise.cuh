#include <cuda_runtime.h>
#include <curand_kernel.h>
#include "math/cudaMath.cuh"


// Distance to a random position in a cell, used for Worley noise
__device__ float getDistanceToCell(int3 pos,
                                   int cellIdx,
                                   int3 originalCellPos,
                                   int3 celloffset,
                                   int totalCells,
                                   float cellSize,
                                   unsigned long long seed,
                                   curandState* state)
{
    // Addition of cellIdx so that every cell has a different seed, but we are able to get the same result by grabbing their
    // cell idx
    //const int cellIdx = cellPos.x + cellPos.y * totalCells + cellPos.z * totalCells * totalCells;
    curand_init(seed + cellIdx, 0, 0, state);

    // Get 3 random coordinates in this cell
    float3 randomPosInCell{};
    randomPosInCell.x = curand_uniform(state) * cellSize;
    randomPosInCell.y = curand_uniform(state) * cellSize;
    randomPosInCell.z = curand_uniform(state) * cellSize;

    // Add correct offset in space
    randomPosInCell = randomPosInCell +
                      (make_float3(originalCellPos.x * cellSize, originalCellPos.y * cellSize, originalCellPos.z * cellSize) +
                       make_float3(celloffset.x * cellSize, celloffset.y * cellSize, celloffset.z * cellSize));
    return distance(make_float3(float(pos.x), float(pos.y), float(pos.z)), randomPosInCell);
}

// Create a random vector on x and y based on seed
__device__ float3 randomVector(const int seed, const int x, const int y, const int z)
{
    // Using Hash to get a random number
    unsigned int h = x * 374761393u + y * 668265263u + z * 1274126177u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);

    float theta = (h & 0xFFFF) / 65535.0f * 2.0f * 3.14159265f;
    float phi = acosf(1.0f - 2.0f * ((h >> 16) & 0xFFFF) / 65535.0f);

    // Create vector form the angles
    float3 test = make_float3(sinf(phi) * cosf(theta), cosf(phi), sinf(phi) * sinf(theta));

    //    if (x > width / 2 - 5 && x < width / 2 + 5 && y == 0 && z == 0) printf("x %i y %i z %i, theta %f phi %f test: x %f y
    //    %f z %f, width %i\n", x, y, z, theta, phi, test.x, test.y, test.z, width);
    return test;
}

// Get random vector and return dot product
__device__ float dotGridGradient(const int seed, int ix, int iy, int iz, const float dx, const float dy, const float dz, int resolution)
{
    ix = ((ix % resolution) + resolution) % resolution;
    iy = ((iy % resolution) + resolution) % resolution;
    iz = ((iz % resolution) + resolution) % resolution;

    // Get vector from int coordinate
    float3 gradient = normalize(randomVector(seed, ix, iy, iz));

    return dx * gradient.x + dy * gradient.y + dz * gradient.z;
}

// Used in perlin noise texture creation to get random noise based on random vector on coordinate
__device__ float perlin(const int seed, float x, float y, float z, int resolution)
{
    x = fmodf(x, float(resolution));
    y = fmodf(y, float(resolution));
    z = fmodf(z, float(resolution));
    if (x < 0.0f) x += resolution;
    if (y < 0.0f) y += resolution;
    if (z < 0.0f) z += resolution;

    const int x0 = int(floorf(x));
    const int y0 = int(floorf(y));
    const int z0 = int(floorf(z));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;

    // Determine weights for the interpolation
    float sx = x - float(x0);
    float sy = y - float(y0);
    float sz = z - float(z0);

    // Compute and interpolate the top two corners
    float n0 = dotGridGradient(seed, x0, y0, z0, sx, sy, sz, resolution);
    float n1 = dotGridGradient(seed, x1, y0, z0, sx - 1.0f, sy, sz, resolution);
    float ix0 = interpolate(n0, n1, sx);

    // Compute and interpolate the bottom two corners
    n0 = dotGridGradient(seed, x0, y1, z0, sx, sy - 1.0f, sz, resolution);
    n1 = dotGridGradient(seed, x1, y1, z0, sx - 1.0f, sy - 1.0f, sz, resolution);
    float ix1 = interpolate(n0, n1, sx);

    // Compute and interpolate the top two backward corners
    n0 = dotGridGradient(seed, x0, y0, z1, sx, sy, sz - 1.0f, resolution);
    n1 = dotGridGradient(seed, x1, y0, z1, sx - 1.0f, sy, sz - 1.0f, resolution);
    const float iz0 = interpolate(n0, n1, sx);

    // Compute and interpolate the bottom backward two corners
    n0 = dotGridGradient(seed, x0, y1, z1, sx, sy - 1.0f, sz - 1.0f, resolution);
    n1 = dotGridGradient(seed, x1, y1, z1, sx - 1.0f, sy - 1.0f, sz - 1.0f, resolution);
    const float iz1 = interpolate(n0, n1, sx);

    // interpolate the forward two
    ix0 = interpolate(ix0, iz0, sz);

    // interpolate the backward two
    ix1 = interpolate(ix1, iz1, sz);

    // Interpolate between these two points
    return interpolate(ix0, ix1, sy);
}


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
                                     unsigned long long seed = 1)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= resolution || y >= resolution || z >= resolution) return;

    const int idx = x + y * resolution + z * resolution * resolution;
    float worleyValue = 0.0f;

    // Randomizer
    curandState state;
    curand_init(seed, 0, 0, &state);

    // ---------------------Worley noise---------------------
   
    // Amount of points in one cell
    float pointsPerCell = float(resolution) / float(samplePoints);

    // At which cell we are precise
    float cellX = float(x) / float(resolution) * float(samplePoints);
    float cellY = float(y) / float(resolution) * float(samplePoints);
    float cellZ = float(z) / float(resolution) * float(samplePoints);

    // Converted to int to get index
    int icellX = int(cellX);
    int icellY = int(cellY);
    int icellZ = int(cellZ);

    // Get distance to all surrounding cells and use the closest one
    float closestDist = 1e16f;
    int3 cellPos = make_int3(x, y, z);

    // We have to do some boundary checks, but wrap around to make sure the texture is tileable

    for (int zi = -1; zi <= 1; zi++)
    {
        for (int yi = -1; yi <= 1; yi++)
        {
            for (int xi = -1; xi <= 1; xi++)
            {
                // Wrapped coordinates to get same seed offset 
                const int wrappedX = ((icellX + xi) % samplePoints + samplePoints) % samplePoints;
                const int wrappedY = ((icellY + yi) % samplePoints + samplePoints) % samplePoints;
                const int wrappedZ = ((icellZ + zi) % samplePoints + samplePoints) % samplePoints;

                // Using ((x % y) + y) % y to correctly wrap around.
                closestDist = fminf(closestDist, 
                          getDistanceToCell(cellPos,
                                            wrappedX + wrappedY * samplePoints + wrappedZ * samplePoints * samplePoints,
                                            make_int3(icellX, icellY, icellZ),
                                            make_int3(xi, yi, zi),
                                            samplePoints,
                                            pointsPerCell,
                                            seed,
                                            &state));

                // if (idx == 0)
                //      printf("x %i y %i z %i, cellIDx %i, idx %i output[idx] %f, closestdist %f\n",
                //             xi,
                //             yi,
                //             zi,
                //            ((xi % samplePoints) + samplePoints) % samplePoints +
                //                ((yi % samplePoints) + samplePoints) % samplePoints * samplePoints +
                //                ((zi % samplePoints) + samplePoints) % samplePoints * samplePoints * samplePoints,
                //
                //             idx,
                //             output[idx],
                //             closestDist);
            }
        }
    }

    // Final result is our smallest distance to our max distance (simplified distance calculation), also invert it
    worleyValue = (closestDist / (sqrtf(pointsPerCell * pointsPerCell * 3)));

    // if (idx < resolution)
    //     printf("x %i y %i z %i, res %i, idx %i output[idx] %f, closestdist %f\n",
    //            x,
    //            y,
    //            z,
    //            resolution,
    //            idx,
    //            output[idx],
    //            closestDist);



    // Then use current output and contribution to possibly include multiple layers

    output[idx] = output[idx] * (1.0f - contribution) +
                  (worleyValue * contribution);
}

__global__ void combineWithPerlin(float* output, const int resolution, const int seed)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= resolution || y >= resolution || z >= resolution) return;

    const int idx = x + y * resolution + z * resolution * resolution;

    // ---------------------Perlin noise---------------------

    // Code and theory from https://www.youtube.com/watch?v=kCIaHqb60Cw
    // TODO: what should the base frequenty be?
    float perlinValue = 0.0f;
    const int octaves = 8;
    const int FREQ0 = resolution / 4;
    float freq = 1.0f;
    float amp = 1.0f;

    for (int i = 0; i < octaves; i++)
    {
        // We need to wrap based on the biggest coordinate possible within a frequency
        int tilePeriod = fmaxf(1, int(float(resolution) / FREQ0 * freq));

        perlinValue += perlin(seed,
                              float(x) * freq / float(FREQ0),
                              float(y) * freq / float(FREQ0),
                              float(z) * freq / float(FREQ0),
                              tilePeriod) *
                       amp;

        freq *= 2.0f;
        amp /= 2.0f;
    }
    // Map -1 to 1 to 0 to 1
    perlinValue = (perlinValue + 1.0f) * 0.5f;

    // Remap perlin noise into the other noise, invert worley noise
    output[idx] = fmaxf(0.0f, remap(perlinValue, 1.0f - output[idx], 1.0f, 0.0f, 1.0f));
}

// code from https://www.shadertoy.com/view/4fX3D8
float __device__ alligator(float3 position, const int gridSize, int seed)
{
    // Scale position to gridsize
    position = position * float(gridSize);

    // floored Position (int pos)
    float3 iPos = floor3f(position);
    // Fractional pos
    float3 fPos = position - make_float3(iPos.x, iPos.y, iPos.z);

    float densest = 0.0f;
    float secondDensest = 0.0f;

    // Compare value to neighbours
    for (int zi = -1; zi <= 1; zi++)
    {
        for (int yi = -1; yi <= 1; yi++)
        {
            for (int xi = -1; xi <= 1; xi++)
            {
                float3 offset = make_float3(float(xi), float(yi), float(zi));
                float3 cellIdx = iPos + offset;

                // Make sure the noise tiles
                cellIdx = make_float3(modClammed(cellIdx.x, float(gridSize)),
                                      modClammed(cellIdx.y, float(gridSize)),
                                      modClammed(cellIdx.z, float(gridSize)));

                cellIdx = cellIdx + make_float3(seed, seed, seed);

                // Get random center of the cell in 3D space
                float3 center{};
                unsigned int rand = randomHash(unsigned(cellIdx.x), unsigned(cellIdx.y), unsigned(cellIdx.z), 0u);
                center.x = (rand & 0xFFFF) / 65535.0f;
                rand = randomHash(unsigned(cellIdx.x), unsigned(cellIdx.y), unsigned(cellIdx.z), 1u);
                center.y = (rand & 0xFFFF) / 65535.0f;
                rand = randomHash(unsigned(cellIdx.x), unsigned(cellIdx.y), unsigned(cellIdx.z), 2u);
                center.z = (rand & 0xFFFF) / 65535.0f;

                float dist = distance(fPos, center + offset);
                // Smooth value using ( https://www.desmos.com/calculator/un0o21eokv )
                dist = clampf(1.0f - dist, 0.0f, 1.0f); 
                dist = dist * dist * (3.0f - 2.0f * dist);

                rand = randomHash(unsigned(cellIdx.x), unsigned(cellIdx.y), unsigned(cellIdx.z), 3u);
                float density = ((rand & 0xFFFF) / 65535.0f) * dist;

                // Find densest value
                if (density >= densest)
                {
                    secondDensest = densest;
                    densest = density;
                }
                else if (density >= secondDensest)
                {
                    secondDensest = density;
                }

            }
        }
    }
    // Subtract two biggest densities
    return densest - secondDensest;
}

// code from https://www.shadertoy.com/view/4fX3D8 
void __global__ alligatorNoise(float* output, int resolution, int gridSize, int seed, const int octaves, const float lacunarity, const float presistence)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= resolution || y >= resolution || z >= resolution) return;

    const int idx = x + y * resolution + z * resolution * resolution;


    float amp = 1.0f;
    float ampSum = 0.0f;
    float result = 0.0f;
    float3 uvw = make_float3(float(x) / float(resolution), float(y) / float(resolution), float(z) / float(resolution));

    for (int i = 0; i < octaves; i++)
    {
        result += alligator(uvw, gridSize, seed) * amp;

        // Sum up amp to normalize later
        ampSum += amp;

        // Increase frequency 
        gridSize *= lacunarity;

        // Decrease amplitude for each octave
        amp *= presistence;

        // Add unique offset to seed to each octave is different
        seed += gridSize; 
    }
    result /= ampSum;

    // Increase of gamma
    result = clampf(result * 1.5, 0., 1.);
    result = powf(result, 0.7);

    output[idx] = 1.0f - result;
}


