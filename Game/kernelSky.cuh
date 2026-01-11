#pragma once

#include <cuda_runtime.h> 
struct Neigh;

void initKernelSky(const int* _GHeight, const float* _defaultVel);

//-----------------Diffusing----------------

__global__ void diffuseRed(const Neigh* neigh, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal,
	const float* input, float* output, const float k, const int type);
__global__ void diffuseBlack(const Neigh* neigh, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal,
	const float* input, float* output, const float k, const int type);
//------------------------------------------


//-----------------Advecting----------------

__global__ void advectGroundWaterRed(const float* inputQrs, const float* inputQgr, float* Qrs, float* Qgr, const float dt, const float speed);
__global__ void advectGroundWaterBlack(const float* inputQrs, const float* inputQgr, float* Qrs, float* Qgr, const float dt, const float speed);

__device__ float advectPPMFlux(const float* array, const float defaultVal, const float velfield, const Neigh neighbour, const Neigh downWindNeigh,
	const float dt, const int tX, const bool x, const bool isRight);
__global__ void advectPPMX(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neighbour,
	const float dt);
__global__ void advectPPMY(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neighbour,
	const float dt);

__device__ float3 calculateFallingVel(const float* Qr, const float* Qs, const float* Qi, const float densAir, const int type);
__global__ void advectPrecipRed(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* pressures, const float* groundP, const int type, const float dt);
__global__ void advectPrecipBlack(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* pressures, const float* groundP, const int type, const float dt);
//------------------------------------------


//-------------------Pressure Projection------------------

__device__ void dotProductGPU(float* result, const float* a, const float* b);
__device__ void applyAGPU(float* ouput, const float* input, const char3 A, const char3 ADo, const char3 ALe);
__device__ void applyPreconditionerGPU(float* output, const float precon, const float div);
__device__ float calculateDivergenceGPU(Neigh neigh, float* velX, float* velY);
__global__ void calculatePresProjGPU(float* outputArray, const Neigh* neigh, const float* velX, const float* velY, const float* precon, float* divergence,
	float* z, float* s, const char3* A, float* Sresult1, float* Sresult2);
__global__ void applyPresProjGPU(const float* pressure, const Neigh* neigh, float* velX, float* velY);
//------------------------------------------


//-------------------Other------------------

__global__ void calculateCloudCover(float* output, const float* Qc, const float* Qw);
__global__ void calculateGroundTemp(float* groundT, const float dtSpeed, const float irridiance, const float* LC);
__device__ float calculateLayerAverage(const float* layer, const float* pressures, const float* groundP, const float maxDistance, const int lY, const bool temp);
__global__ void buoyancyGPU(float* velY, const Neigh* neigh, const float* potTemp, const float* Qv,
	const float* pressures, const float* groundP, float* buoyancyStor, const float maxDistance, const float dt);
__global__ void addHeatGPU(const float* _Qv, float* potTemp, float* condens, float* depos, float* freeze);
__global__ void computeNeighbourGPU(Neigh* neigh);
__global__ void initAandPrecon(char3* A, float* precon, const Neigh* neigh);

//------------------------------------------


//------------------Helper------------------

__global__ void resetVelPressProj(const Neigh* neigh, float* velX, float* velY);
__device__ bool isGroundLevel(); //Uses the thread idxs
__device__ bool isGroundGPU(); //Uses the thread idxs
__device__ bool isGroundGPU(const int x, const int y);
//------------------------------------------
