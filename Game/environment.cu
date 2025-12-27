#include <sstream>
#include <iostream>

#include <cassert>
#include <memory>

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "environment.h"
#include "environment.cuh"

#define BLOCKSIZE GRIDSIZESKYX
#define BLOCKSIZEY GRIDSIZESKYY

environmentGPU::environmentGPU()
{
	//Malloc space for the pointers
	cudaMalloc((void**)&m_array, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_outputArray, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_defaultVal, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_velfieldX, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_velfieldY, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_neighbourData, GRIDSIZESKY * sizeof(Neigh));
}

environmentGPU::~environmentGPU()
{
	//Free space
	cudaFree(m_neighbourData);
	cudaFree(m_velfieldY);
	cudaFree(m_velfieldX);
	cudaFree(m_defaultVal);
	cudaFree(m_outputArray);
	cudaFree(m_array);
}

__device__ float environmentGPU::advectPPMFlux(float* array, float defaultVal, float velfield, Neigh neighbour, Neigh downWindNeigh, float dt, int tX, bool x, bool isRight)
{
	//Inputted array has size of 1 block

	//Dir is same for x and y due to block being 1D array
	const int dir = isRight ? 1 : -1;

	float veli = velfield;

	//Calculate C (Courant number)
	float C = veli * dt / VOXELSIZE;
	C = C > 1.0f ? 1.0f : (C < -1.0f ? -1.0f : C);

	//Depending on upwind/downwind, we use i - 1 or i + 1
	//Downwind with right face = coming from right
	//Downwind with left face = coming from left
	bool downWind = isRight ? C < 0.0f : C > 0.0f;
	const int i = downWind ? tX + dir : tX;
	float qi{ 0.0f };
	float qRight{ 0.0f };
	float qLeft{ 0.0f };

	//Winds from outside the grid, we already returned if left faced. 
	if ((isRight && downWind && ((x && neighbour.right != SKY) || (!x && neighbour.up != SKY))) ||
		(!isRight && downWind && ((x && neighbour.left != SKY) || (!x && neighbour.down != SKY))))
	{
		qi = qRight = qLeft = defaultVal;
	}
	else if (x)
	{
		qi = array[i];
		qRight = downWindNeigh.right != SKY ? array[i] : array[i + 1];
		qLeft = downWindNeigh.left != SKY ? array[i] : array[i - 1];
	}
	else
	{
		qi = array[i];
		qRight = downWindNeigh.up != SKY ? array[i] : array[i + 1];
		qLeft = downWindNeigh.down != SKY ? array[i] : array[i - 1];
	}

	const float qMin = cuda::std::fmin(qLeft, cuda::std::fmin(qi, qRight));
	const float qMax = cuda::std::fmax(qLeft, cuda::std::fmax(qi, qRight));

	//Slope limiter: van Leer
	float s = 0.0f;
	const float diffLeft = qi - qLeft;
	const float diffRight = qRight - qi;

	//Check if qi is NOT a local extremum (higher or lower then both neighbours)
	//Meaning, if (1,2,3), this would be true. In the case (1,2,1), it would be false.
	if (diffLeft * diffRight > 0.0f)
	{
		//This we do to make sure the slope does not overshoot left or right.
		//       
		//    Overshoot  
		//        /-\______
		//       /
		//      /
		//---\_/
		//  Undershoot
		//     
		const float diffBoth = qRight - qLeft;
		const float absLeft = cuda::std::fabsf(diffLeft);
		const float absRight = cuda::std::fabsf(diffRight);
		const float absBoth = cuda::std::fabsf(diffBoth);

		const float minDiff = cuda::std::fmin(absRight, absLeft);
		const float slopeLimit = cuda::std::fmin(0.5f * absBoth, minDiff);

		//Sign
		s = diffBoth >= 0.0f ? slopeLimit : -slopeLimit;
	}

	//Set initial parabolic edges
	const float qLS = qi - 0.5f * s; //Left edge
	const float qRS = qi + 0.5f * s; //Right edge

	//Monotonicity constrained from Walcek
	float B = 1.0f;

	//If right edge is bigger then max or left edge is smaller then minimum.
	//Can only happen if qi is NOT a local extremum (s = 0.0)
	if (qRS > qMax)
	{
		const float denominator = qRS - qi;
		if (denominator > 1e-16f)
		{
			B = cuda::std::fmin(B, (qMax - qi) / denominator);
		}
	}
	if (qLS < qMin)
	{
		const float denominator = qi - qLS;
		if (denominator > 1e-16f)
		{
			B = cuda::std::fmin(B, (qi - qMin) / denominator);
		}
	}

	//Scale to be in between min and max
	float qR = qi + B * (qRS - qi);
	float qL = qi + B * (qLS - qi);

	//Fix extremum (qi is higher/lower then neighbours) i.e. (1,2,1)
	//We just flatten it...
	if ((qR - qi) * (qi - qL) <= 0.0f)
	{
		qR = qL = qi;
	}


	float flux = 0.0f;
	if (C > 0.0f)
	{
		//a + bx + cx^2 (C being the variable (x))
		const float qq = qR + C * (-qL - 2 * qR + 3 * qi) + C * C * (qL + qR - 2 * qi);
		flux = veli * qq;
	}
	else
	{
		//a + bx + cx^2 (C being the variable (x))
		const float qq = qR + C * (-2 * qL - qR + 3 * qi) + C * C * (qL + qR - 2 * qi);
		flux = veli * qq;
	}

	return flux;
}

__global__ void environmentGPU::advectPPMX(float* __restrict__ arrayIn,
							float* __restrict__ arrayOut, 
							float* __restrict__ defaultVal, 
							float* __restrict__ velfieldX,
							Neigh* __restrict__  neighbour,
							float dt)
{
	__shared__ float sArray[BLOCKSIZE];
	__shared__ float sVelX[BLOCKSIZE];

	int tX = threadIdx.x;
	int idx = blockIdx.x * GRIDSIZESKYX + tX;
	int idxY = idx / GRIDSIZESKYX;

	if (idx >= GRIDSIZESKY) return;

	sArray[tX] = arrayIn[idx];
	sVelX[tX] = velfieldX[idx];

	__syncthreads();

	Neigh neigh = neighbour[idx];
	float defVal = 0.0f;
	if (defaultVal)
	{
		defVal = defaultVal[idxY];
	}

	//Set correct velocities
	float velLeft;
	float velRight = sVelX[tX];
	if (neigh.left == SKY && tX > 0)
	{
		velLeft = sVelX[tX - 1];
	}
	else
	{
		velLeft = sVelX[tX]; //Use center
	}

	//Set downwind neighs
	Neigh neighDownwindR = (velRight < 0.0f && tX < GRIDSIZESKYX - 1) ? neighbour[idx + 1] : neighbour[idx];
	Neigh neighDownwindL = (velLeft > 0.0f && tX > 0) ? neighbour[idx - 1] : neighbour[idx];

	float fluxRight = advectPPMFlux(sArray, defVal, velRight, neigh, neighDownwindR, dt, tX, true, true);
	float fluxLeft = advectPPMFlux(sArray, defVal, velLeft, neigh, neighDownwindL, dt, tX, true, false);

	arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxRight - fluxLeft);
}

__global__ void environmentGPU::advectPPMY(float* __restrict__ arrayIn,
							float* __restrict__ arrayOut,
							float* __restrict__ defaultVal,
							float* __restrict__ velfieldY,
							Neigh* __restrict__  neighbour,
							float dt)
{
	__shared__ float sArray[BLOCKSIZEY];
	__shared__ float sVelY[BLOCKSIZEY];

	int tY = threadIdx.x;
	int idx = tY * GRIDSIZESKYX + blockIdx.x;
	int idxY = idx / GRIDSIZESKYX;

	if (idx >= GRIDSIZESKY) return;

	sArray[tY] = arrayIn[idx];
	sVelY[tY] = velfieldY[idx];

	__syncthreads();

	Neigh neigh = neighbour[idx];
	float defVal = 0.0f;
	if (defaultVal)
	{
		defVal = defaultVal[idxY];
	}

	//Set correct velocities
	float velDown;
	float velUp = sVelY[tY];
	if (neigh.down == SKY && tY > 0)
	{
		velDown = sVelY[tY - 1];
	}
	else
	{
		velDown = sVelY[tY]; //Use center
	}

	//Set downwind neighs
	Neigh neighDownwindU = (velUp < 0.0f && idx < GRIDSIZESKY - GRIDSIZESKYX - 1) ? neighbour[idx + GRIDSIZESKYX] : neighbour[idx];
	Neigh neighDownwindD = (velDown > 0.0f && idx >= GRIDSIZESKYX) ? neighbour[idx - GRIDSIZESKYX] : neighbour[idx];

	float fluxUp = advectPPMFlux(sArray, defVal, velUp, neigh, neighDownwindU, dt, tY, false, true);
	float fluxDown = advectPPMFlux(sArray, defVal, velDown, neigh, neighDownwindD, dt, tY, false, false);

	arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxUp - fluxDown);
}



void environmentGPU::advectPPMWGPU(float* array, float* defaultVal, float* velfieldX, float* velfieldY, Neigh* neighbourData, const float dt)
{
	//For now use just the size, since we have no intend (yet) of going larger than 256 or even 1024.
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;
	//const int totalThreads = GRIDSIZESKY;
	//const int numBlocks = cuda::ceil_div(totalThreads, threads); //Using CUDA function to do the amount of blocks calculation
	//const int numOvertimes = cuda::ceil_div(numBlocks, blocks); //We won't go over the amount of max blocks


	//Create events to track time taking of parts
	//cudaEvent_t start;
	//cudaEvent_t stop;
	//cudaEventCreate(&start);
	//cudaEventCreate(&stop);
	//cudaEventRecord(start);


	//Copying over all data
	cudaMemcpy(m_array, array, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemset(m_outputArray, 0, GRIDSIZESKY * sizeof(float));

	if (defaultVal)
	{
		cudaMemcpy(m_defaultVal, defaultVal, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	}
	else
	{
		cudaMemset(m_defaultVal, 0, GRIDSIZESKY * sizeof(float));
	}

	cudaMemcpy(m_velfieldX, velfieldX, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(m_velfieldY, velfieldY, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(m_neighbourData, neighbourData, GRIDSIZESKY * sizeof(Neigh), cudaMemcpyHostToDevice);

	//Recollect all threads before starting kernels
	cudaDeviceSynchronize();
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "CUDA error 2: " << cudaGetErrorString(err) << std::endl;
		exit(-1);
	}


	//TODO: add offset
	//for (int i = 0; i < numOvertimes; i++)
	{
		//Its kernel time, advecting 2 times half X and 1 time Y
		advectPPMX << <blocks, threads >> > (m_array, m_outputArray, m_defaultVal, m_velfieldX, m_neighbourData, dt * 0.5f);
		cudaDeviceSynchronize();

		advectPPMY << <threads, blocks >> > (m_outputArray, m_array, m_defaultVal, m_velfieldY, m_neighbourData, dt);
		cudaDeviceSynchronize();

		advectPPMX << <blocks, threads >> > (m_array, m_outputArray, m_defaultVal, m_velfieldX, m_neighbourData, dt * 0.5f);
		cudaDeviceSynchronize();
	}

	cudaMemcpy(array, m_outputArray, GRIDSIZESKY * sizeof(float), cudaMemcpyDefault);

	err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "CUDA error 4: " << cudaGetErrorString(err) << std::endl;
		exit(-1);
	}


	//End recording and record time
	//cudaEventRecord(stop);
	//cudaStreamSynchronize(0);
	//float elapsedTime;
	//cudaEventElapsedTime(&elapsedTime, start, stop);
	//std::cout << "Execution time: " << elapsedTime << " ms" << std::endl;
	//cudaEventDestroy(start);
	//cudaEventDestroy(stop);
}
