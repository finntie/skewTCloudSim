#pragma once

#include <cuda_runtime.h> 
#include "config.h"


void initKernelSky(const int* _GHeight, const float* _defaultVelX, const float* _defaultVelZ);

//-----------------Diffusing----------------

__global__ void diffuseRedBlack(const float* groundT, const float* pressures, const float* groundP, const float* defaultVal,
	const float* input, float* output, const float k, const int type, boundsEnv bounds, bool red, Neigh* neighbourInfo);
//------------------------------------------


//-----------------Advecting----------------

__global__ void advectGroundWaterGPU(float* Qrs, float* Qgr);

__global__ void setTempsAtGroundGPU(float* potTemps, const float* groundTemps, const float* pressures, const float* groundPressures, const float dt);
__device__ float advectPPMFlux(const float velocity, const float valL, const float valC, const float valR, const float dt);
__global__ void advectPPMX(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neighbour,
	const boundsEnv bounds,
	const float dt);
__global__ void advectPPMY(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neighbour,
	const boundsEnv bounds,
	const float dt);
__global__ void advectPPMZ(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldZ,
	const Neigh* __restrict__  neighbour,
	const boundsEnv bounds,
	const float dt);

__global__ void advectPrecipGPU(float* Qj, const Neigh* neigh, const float* potTemp, const float* Qv,
	const float* pressures, const float* groundP, const int type, const float dt);
//------------------------------------------


//-------------------Pressure Projection------------------

__global__ void dotProductGPU(float* result, const float* a, const float* b);
__global__ void applyAGPU(float* ouput, const float* input, const Neigh* neigh, const float4* A, boundsEnv bounds);
__global__ void applyPreconditionerGPU(float* output, const float* precon, const float* div, float4* A);
__global__ void calculateDivergenceGPU(float* divergence, const Neigh* neigh, const float* velX, const float* velY, const float* velZ, const float* dens, const float* oldDens, const float* defaultDens, boundsEnv boundsDens, boundsEnv boundsVel);
__global__ void applyPresProjGPU(const float* pressure, const Neigh* neigh, float* velX, float* velY, float* velZ, const float* density, const float* pressureEnv, boundsEnv boundsDens, const float dt);
__global__ void getMaxDivergence(float* output, const float* div);
__global__ void updatePandDiv(float* S1, float* S2, float* pressure, float* divergence, const float* s, const float* z);
__global__ void endIteration(float* S1, float* S2, float* s, const float* z);
__global__ void updatePressure(float* envPressure, const float* presProj);
//------------------------------------------


//-------------------Other------------------

__global__ void calculateCloudCoverGPU(float* output, const float* Qc, const float* Qw, const int* GHeight);
__global__ void calculateGroundTempGPU(float* groundT, const float dtSpeed, const float irridiance, const float* LC);
__global__ void buoyancyGPU(float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* defTemp, const float* defQv, const float* pressures, const float* groundP, float* buoyancyStor, boundsEnv bounds, boundsEnv boundsVelY);
__global__ void addHeatGPU(const float* _Qv, float* potTemp, float* condens, float* depos, float* freeze);
__global__ void computeNeighbourGPU(Neigh* neigh);
__global__ void initAMatrix(float4* A, const Neigh* neigh, const float* density, const float* defDens, boundsEnv bounds);
__global__ void initPrecon(float* precon, const float4* A);

__global__ void initDensity(float* densityAir, const float* potTemp, const float* pressures, const float* Qv, const float* groundP);
__global__ void calculateNewPressure(float* pressureEnv, const float* densityAir, const float* potTemp, const float* Qv, const float* GPressure);

//------------------------------------------


//-------------------Editor------------------

__global__ void applyBrushGPU(float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const float brushSize, const int3 mousePos,
	const float brushSmoothnes, const float brushIntensity, const float applyValue, const float3 valueDir, const bool groundErase, const float dt);
__global__ void applySelectionGPU(float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const int3 minPos, const int3 maxPos,
	const float applyValue, const float3 valueDir, const bool groundErase);
__device__ bool setGround(int* groundHeight, const int x, const int y, const int z, const bool ground);
__global__ void compareAndResetValuesOutGround(const int* oldGroundHeight, const int* newGroundHeight, const float* isentropicTemp, const float* isentropicVap,
	float* Qv, float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* potTemp, float* velX, float* velY, float* velZ, float* pressure, float* defaultPressure);

//------------------------------------------


//------------------Helper------------------

__global__ void resetVelPressProj(const Neigh* neigh, float* velX, float* velY, float* velZ);
__device__ bool isGroundLevel(const int z); //Uses the thread idxs
__device__ bool isGroundGPU(const int z); //Uses the thread idxs
__device__ bool isGroundGPU(const int x, const int y, const int z);

__global__ void setToDefault(float* array, const float* defaultValue);

// Get avarage velocity at index due to use of MAC grid, direction telling if we want X, Y or Z velocity
__device__ __noinline__ float getVelAtIdx(const Neigh* neigh, boundsEnv boundaryCondition, direction XYZ, const float* velocityField, const float customData, const int idx);

__device__ __noinline__ void fillSharedNeigh(const Neigh* neigh, float* sharedData, const float* data, const float* customData, const int z, boundsEnv boundaryConditions);

__device__ __noinline__ float fillNeighbourData(const singleNeigh neighbourType, boundsEnv boundaryConditions, const float* data, const int idx, const int offset, const float customData, bool up = false);

__device__ __noinline__ void fillDataBoundCon(boundCon condition, float& output, const float data, const float customData);

__device__ __noinline__ float getValueExtraDirShared(const Neigh* neigh, const float* data, const float* sharedData, const int idx, const int idxShared, const int offset, const int offsetShared, direction dir);


__device__ __noinline__ float getValueExtraForward(const Neigh* neigh, boundsEnv boundaryConditions, const float* data, const float customData, const int x, const int y, const int z);

//------------------------------------------
