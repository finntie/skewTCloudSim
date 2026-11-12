#pragma once

#include "config.hpp"


void initKernelSky(simParams p, const float* _defaultVelX, const float* _defaultVelZ, void* stream);
void setConstantsKernelSky(float _simSpeed, float _simDt, float _invBlockSpreadDepth, void* stream);

//-----------------Diffusing----------------

__global__ void diffuseRedBlack(simParams p, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal,
	const float* input, float* output, const float* k, const int type, boundsEnv bounds, bool red, Neigh* neighbourInfo);
//------------------------------------------


//-----------------Advecting----------------

__global__ void advectGroundWaterGPU(simParams p, const int* GHeight, float* Qrs, float* Qgr);

__global__ void setTempsAtGroundGPU(simParams p, const int* GHeight, float* potTemps, const float* groundTemps, const float* pressures, const float* groundPressures, const float dt);
__global__ void advectPPMX(simParams p, 
	const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neighbour,
	const int* __restrict__ GHeight,
	const boundsEnv bounds,
	const float* dt);
__global__ void advectPPMY(simParams p, 
	const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neighbour,
	const int* __restrict__ GHeight,
	const boundsEnv bounds,
	const float* dt);
__global__ void advectPPMZ(simParams p, 
	const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldZ,
	const Neigh* __restrict__  neighbour,
	const int* __restrict__ GHeight,
	const boundsEnv bounds,
	const float* dt);

__global__ void advectPrecipGPU(simParams p, const int* GHeight, float* Qj, const Neigh* neigh, const float* potTemp, const float* Qv,
	const float* pressures, const float* groundP, const int type, const float dt);
//------------------------------------------


//-------------------Pressure Projection------------------

__global__ void dotProductGPU(simParams p, const int* GHeight, float* result, const float* a, const float* b);
__global__ void applyAGPU(simParams p, float* ouput, const float* input, const Neigh* neigh, const float4* A);
__global__ void applyPreconditionerGPU(simParams p, const int* GHeight, float* output, const float* precon, const float* div, float4* A);
__global__ void calculateDivergenceGPU(simParams p, const int* GHeight, float* divergence, const Neigh* neigh, const float* velX, const float* velY, const float* velZ, const float* dens, const float* oldDens, const float* defaultDens);
__global__ void applyPresProjGPU(simParams p, const int* GHeight, const float* pressure, const Neigh* neigh, float* velX, float* velY, float* velZ, const float* density, const float* pressureEnv, const float dt, float* m_stor0);
__global__ void getMaxDivergence(simParams p, const int* GHeight, float* output, const float* div);
__global__ void updatePandDiv(simParams p, const int* GHeight, float* S1, float* S2, float* pressure, float* divergence, const float* s, const float* z);
__global__ void endIteration(simParams p, const int* GHeight, float* S1, float* S2, float* s, const float* z);
__global__ void updatePressure(simParams p, const int* GHeight, float* envPressure, const float* presProj);
//------------------------------------------


//-------------------Ground------------------

__global__ void initGroundHeightGPU(simParams p, int* _GHeight, const float* noise, const float maxHeight);
__global__ void resetValueInGround(simParams p, float* array, const int* _GHeight);
__global__ void computeIsenTempGroundGPU(simParams p, float* array, const float* isenTemp, const float* groundPressure, const float* pressures, const int* _GHeight);
//------------------------------------------


//-------------------Other------------------

__global__ void calculateCloudCoverGPU(simParams p, float* output, const float* Qc, const float* Qw, const int* GHeight);
__global__ void calculateGroundTempGPU(simParams p, float* groundT, const float dtSpeed, const float irridiance, const float* LC);
__global__ void buoyancyGPU(simParams p, const int* GHeight, float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* defTemp, const float* defQv, const float* pressures, const float* groundP, float* buoyancyStor);
__global__ void addHeatGPU(simParams p, const float* _Qv, float* potTemp, float* condens, float* depos, float* freeze);
__global__ void computeNeighbourGPU(simParams p, const int* GHeight, Neigh* neigh);
__global__ void initAMatrix(simParams p, const int* GHeight, float4* A, const Neigh* neigh, const float* density, const float* defDens);
__global__ void initPrecon(simParams p, const int* GHeight, float* precon, const float4* A);

__global__ void initDensity(simParams p, const int* GHeight, float* densityAir, const float* potTemp, const float* pressures, const float* Qv, const float* groundP);
__global__ void calculateNewPressure(simParams p, const int* GHeight, float* pressureEnv, const float* densityAir, const float* potTemp, const float* Qv, const float* GPressure);

//------------------------------------------


//-------------------Editor------------------

__global__ void applyBrushGPU(simParams p, const int* GHeight, float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const float brushSize, const int3 mousePos,
	const float brushSmoothnes, const float brushIntensity, const float applyValue, const float3 valueDir, const bool groundErase, const float dt);
__global__ void applySelectionGPU(simParams p, const int* GHeight, float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const int3 minPos, const int3 maxPos,
	const float applyValue, const float3 valueDir, const bool groundErase);
__global__ void compareAndResetValuesOutGround(simParams p, const int* oldGroundHeight, const int* newGroundHeight, const float* isentropicTemp, const float* isentropicVap,
	float* Qv, float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* potTemp, float* velX, float* velY, float* velZ, float* pressure, float* defaultPressure);

//------------------------------------------


//------------------Helper------------------

__global__ void resetVelPressProj(simParams p, const int* GHeight, const Neigh* neigh, float* velX, float* velY, float* velZ);


__global__ void setToDefault(simParams p, const int* GHeight, float* array, const float* defaultValue);


//------------------------------------------
