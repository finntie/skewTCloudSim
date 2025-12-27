#ifndef ADVECTION_KERNEL_CUH
#define ADVECTION_KERNEL_CUH

#include <cuda_runtime.h> 
#include <glm/glm.hpp>
struct Neigh;



class environmentGPU
{
public:

	environmentGPU();
	~environmentGPU();

	//-----------------Diffusing----------------





	//------------------------------------------


	//-----------------Advecting----------------

	__device__ float advectPPMFlux(float* array, float defaultVal, float velfield, Neigh neighbour, Neigh downWindNeigh, float dt, int tX, bool x, bool isRight);
	__global__ void advectPPMX(float* __restrict__ arrayIn,
		float* __restrict__ arrayOut,
		float* __restrict__ defaultVal,
		float* __restrict__ velfieldX,
		Neigh* __restrict__  neighbour,
		float dt);
	__global__ void advectPPMY(float* __restrict__ arrayIn,
		float* __restrict__ arrayOut,
		float* __restrict__ defaultVal,
		float* __restrict__ velfieldY,
		Neigh* __restrict__  neighbour,
		float dt);

	void advectPPMWGPU(float* array, float* defaultVal, float* velfieldX, float* velfieldY, Neigh* neighbourData, const float dt);

	//------------------------------------------

private:

	float* m_array;
	float* m_outputArray;
	float* m_defaultVal;
	float* m_velfieldX;
	float* m_velfieldY;
	Neigh* m_neighbourData;




};


#endif
