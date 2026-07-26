#pragma once

#include <cuda_runtime.h> 
#include "config.h"


void initKernelSky(const float* _defaultVelX, const float* _defaultVelZ, void* stream);

//-----------------Diffusing----------------

__global__ void diffuseRedBlack(const float* groundT, const float* pressures, const float* groundP, const float* defaultVal,
	const float* input, float* output, const float k, const int type, boundsEnv bounds, bool red, Neigh* neighbourInfo);
//------------------------------------------


//-----------------Advecting----------------

__global__ void advectGroundWaterGPU(const int* GHeight, float* Qrs, float* Qgr);

__global__ void setTempsAtGroundGPU(const int* GHeight, float* potTemps, const float* groundTemps, const float* pressures, const float* groundPressures, const float dt);
__device__ float advectPPMFlux(const float velocity, const float valL, const float valC, const float valR, const float dt);
__global__ void advectPPMX(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neighbour,
	const int* __restrict__ GHeight,
	const boundsEnv bounds,
	const float* dt);
__global__ void advectPPMY(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neighbour,
	const int* __restrict__ GHeight,
	const boundsEnv bounds,
	const float* dt);
__global__ void advectPPMZ(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldZ,
	const Neigh* __restrict__  neighbour,
	const int* __restrict__ GHeight,
	const boundsEnv bounds,
	const float* dt);

__global__ void advectPrecipGPU(const int* GHeight, float* Qj, const Neigh* neigh, const float* potTemp, const float* Qv,
	const float* pressures, const float* groundP, const int type, const float dt);
//------------------------------------------


//-------------------Pressure Projection------------------

__global__ void dotProductGPU(const int* GHeight, float* result, const float* a, const float* b);
__global__ void applyAGPU(float* ouput, const float* input, const Neigh* neigh, const float4* A);
__global__ void applyPreconditionerGPU(const int* GHeight, float* output, const float* precon, const float* div, float4* A);
__global__ void calculateDivergenceGPU(const int* GHeight, float* divergence, const Neigh* neigh, const float* velX, const float* velY, const float* velZ, const float* dens, const float* oldDens, const float* defaultDens);
__global__ void applyPresProjGPU(const int* GHeight, const float* pressure, const Neigh* neigh, float* velX, float* velY, float* velZ, const float* density, const float* pressureEnv, const float dt, float* m_stor0);
__global__ void getMaxDivergence(const int* GHeight, float* output, const float* div);
__global__ void updatePandDiv(const int* GHeight, float* S1, float* S2, float* pressure, float* divergence, const float* s, const float* z);
__global__ void endIteration(const int* GHeight, float* S1, float* S2, float* s, const float* z);
__global__ void updatePressure(const int* GHeight, float* envPressure, const float* presProj);
//------------------------------------------


//-------------------Other------------------

__global__ void calculateCloudCoverGPU(float* output, const float* Qc, const float* Qw, const int* GHeight);
__global__ void calculateGroundTempGPU(float* groundT, const float dtSpeed, const float irridiance, const float* LC);
__global__ void buoyancyGPU(const int* GHeight, float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* defTemp, const float* defQv, const float* pressures, const float* groundP, float* buoyancyStor);
__global__ void addHeatGPU(const float* _Qv, float* potTemp, float* condens, float* depos, float* freeze);
__global__ void computeNeighbourGPU(const int* GHeight, Neigh* neigh);
__global__ void initAMatrix(const int* GHeight, float4* A, const Neigh* neigh, const float* density, const float* defDens);
__global__ void initPrecon(const int* GHeight, float* precon, const float4* A);

__global__ void initDensity(const int* GHeight, float* densityAir, const float* potTemp, const float* pressures, const float* Qv, const float* groundP);
__global__ void calculateNewPressure(const int* GHeight, float* pressureEnv, const float* densityAir, const float* potTemp, const float* Qv, const float* GPressure);

//------------------------------------------


//-------------------Editor------------------

__global__ void applyBrushGPU(const int* GHeight, float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const float brushSize, const int3 mousePos,
	const float brushSmoothnes, const float brushIntensity, const float applyValue, const float3 valueDir, const bool groundErase, const float dt);
__global__ void applySelectionGPU(const int* GHeight, float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const int3 minPos, const int3 maxPos,
	const float applyValue, const float3 valueDir, const bool groundErase);
__device__ bool setGround(const int* GHeight, int* groundHeight, const int x, const int y, const int z, const bool ground);
__global__ void compareAndResetValuesOutGround(const int* oldGroundHeight, const int* newGroundHeight, const float* isentropicTemp, const float* isentropicVap,
	float* Qv, float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* potTemp, float* velX, float* velY, float* velZ, float* pressure, float* defaultPressure);

//------------------------------------------


//------------------Helper------------------

__global__ void resetVelPressProj(const int* GHeight, const Neigh* neigh, float* velX, float* velY, float* velZ);
__forceinline__ __device__ bool isGroundLevel(const int* GHeight, const int z); //Uses the thread idxs
__forceinline__ __device__ bool isGroundGPU(const int* GHeight, const int z); //Uses the thread idxs
__forceinline__ __device__ bool isGroundGPU(const int* GHeight, const int x, const int y, const int z);

__global__ void setToDefault(const int* GHeight, float* array, const float* defaultValue);

// Get avarage velocity at index due to use of MAC grid, direction telling if we want X, Y or Z velocity
__device__ float getVelAtIdx(const Neigh neigh, const boundsEnv& boundaryCondition, direction XYZ, const float* velocityField, const float customData, const int idx);

__device__ __forceinline__ void fillSharedNeigh(const Neigh neigh, float* sharedData, const float* data, const float customData, const int z, const boundsEnv& boundaryConditions);

__device__ __forceinline__ float fillNeighbourData(const bool neighbourOutside, const envType type, const boundsEnv& boundaryConditions, const float* data, const int idx, const int offset, const float customData, bool up = false);

__device__ __forceinline__ void fillDataBoundCon(boundCon condition, float& output, const float data, const float customData);

__device__ __forceinline__ float getValueExtraDirShared(const Neigh* neigh, const float* data, const float* sharedData, const int idx, const int idxShared, const int offset, const int offsetShared, direction dir);


__device__ __forceinline__ float getValueExtraForwardBackward(const Neigh* neigh, const boundsEnv& boundaryConditions, const float* data, const float customData, const int x, const int y, const int z, bool forward = true);

//------------------------------------------
