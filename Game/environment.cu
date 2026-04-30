#include <sstream>
#include <iostream>

#include <cassert>
#include <memory>

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "environment.cuh"
#include "environment.h"
#include "kernelSky.cuh"
#include "kernelGround.cuh"
#include "microPhysics.cuh"
#include "dataClass.cuh"
#include "utils.cuh"

#include "math/meteoformulas.h"
#include "meteoformulas.cuh"
#include "kernelMaths.cuh"
#include "math/geometry.hpp"

#include "editor.h"
#include "game.h"


// Constant simulation variables (extern in utils.cuh)
__constant__ int simSizeX{ GRIDSIZESKYX };
__constant__ int simSizeY{ GRIDSIZESKYY };
__constant__ int simSizeZ{ GRIDSIZESKYZ };
__constant__ int simSize{ GRIDSIZESKY };
__constant__ float voxelSize{ VOXELSIZE };
__constant__ int simSizeGround{ GRIDSIZEGROUND };
__constant__ float simSpeed{ 1.0f };
__constant__ float simDeltaTime{ 0.0f };
__constant__ float invBlockSpreadDepth{ 1.0f };


namespace ConstantsGPU
{
	__constant__ float Lb = -0.0065f; //Standard lapse rate in K/m
	__constant__ float g = 9.8076f; //Earth's gravitational acceleration in m/s2
	__constant__ float Hv = 2501000; //Heat vaporation of water in J/kg (also written as L) what about 2501000?2268000 https://library.wmo.int/viewer/59923/?offset=#page=220&viewer=picture&o=search&n=0&q=Lv
	__constant__ float R = 8.3145f; //Universal gas constant in J/mol*k
	__constant__ float Rsd = 287.0528f; //specific gas constant of dry air in J/kg*K
	__constant__ float Rsw = 461.5f; //specific gas constant of water vapor in J/kg*K
	__constant__ float E = 287.0528f / 461.5f; //(Rsd / Rsw) the dimensionless ratio of the specific gas constant of dry air to the specific gas constant for water vapour
	__constant__ float Cvv = 1418.0f; // specific heat	capacity of water vapor at constant volume
	__constant__ float Cva = 719.0f; // (Probably) The specific heat capicity of dry air? (refering to https://escholarship.org/content/qt0d72911v/qt0d72911v.pdf?t=pghwe7)
	__constant__ float Cpa = 287.0528f + 719.0f; // (Rsd + Cva) specific heat capacity at constant pressure for dry air in j/kg*K
	__constant__ float Cpd = 1003.5f; //The specific heat of dry air at constant pressure in j/kg*K
	__constant__ float Cpv = 717.0f; //The specific heat of dry air at constant Volume in j/kg*K
	__constant__ float Cpvw = 1418.0f + 461.5f; //(Cvv + Rsw) the specific heat capacity of water vapor at constant pressure
	__constant__ float Cvl = 4119.0f; //Specific heat capacity at constant volume for liquid water in J/kg*K
	__constant__ float Cpi = 2093.0f; //Specific heat of ice in J/kg/K
	__constant__ float Cpds = 800.0f; //Specific heat capacity of dry soil
	__constant__ float Cpws = 1480.0f; // Specific heat capacity of wet soil
	__constant__ float Mda = 28.966f; //Molair mass of dry air at constant pressure in g/mol
	__constant__ float Mw = 18.02f; //Molair mass of water in g/mol
	__constant__ float ptrip = 611.2f; //Triple point of water in pascal (6.11657 hPa)
	__constant__ float Ttrip = 273.16f; //Triple point of water in Kelvin
	__constant__ float E0v = 2.374e+6f; // Heat latency of vaporising water in J/kg
	__constant__ float Lf = 3.3355e+5f;  // Heat latency of Fusion of water in J/kg
	__constant__ float Ls = 2.834e+6f;  // Heat latency of deposition of water in J/kg
	__constant__ float E0s = 0.3337e+6f; // The difference in specific internal energy between liquid and solid at the triple point. in J/kg
	__constant__ float euler = 2.7182818284f; //Euler's number
	__constant__ float Ka = 2.40e-2f; // thermal conductivity of air in J/m/s/K
	__constant__ float PI = 3.14159265359f;
	__constant__ float oo = 5.67e-8f; // Boltzmann constant in W / m-2 / K-4
	__constant__ float ge = 0.95f; // Ground emissivity
}


environmentGPU::environmentGPU()
{
	// Set block and grid dimension based on GPU specs.
	cudaDeviceProp prop;
	cudaGetDeviceProperties(&prop, 0);
	unsigned int totalThreadsPerBlock = prop.maxThreadsPerBlock;
	unsigned int totalBlocksPerGrid = prop.maxBlocksPerMultiProcessor * prop.multiProcessorCount;
	unsigned int totalRegsPerThread = prop.regsPerBlock / totalThreadsPerBlock;
	// Set block dimensions to be square root of the max threads available. Making use of as much threads as possible
	const unsigned int threadsBlock = uint32_t(floor(sqrt(totalThreadsPerBlock)));
	// If one side is smaller than 1 block, we still want to use as many threads as possible per block
	if (threadsBlock > GRIDSIZESKYX)
	{
		blockDim.x = GRIDSIZESKYX;
		blockDim.y = std::min(uint32_t(GRIDSIZESKYY), uint32_t(double(totalThreadsPerBlock) / double(blockDim.x)));
	}
	else if (threadsBlock > GRIDSIZESKYY)
	{
		blockDim.y = GRIDSIZESKYY;
		blockDim.x = std::min(uint32_t(GRIDSIZESKYX), uint32_t(double(totalThreadsPerBlock) / double(blockDim.y)));
	}
	else
	{
		blockDim.x = std::min(uint32_t(GRIDSIZESKYX), threadsBlock);
		blockDim.y = std::min(uint32_t(GRIDSIZESKYY), threadsBlock);
	}

	// Amount of blocks on the x and y axis is determined by how big our grid is
	gridDim.x = unsigned int(ceil(double(GRIDSIZESKYX) / double(blockDim.x)));
	gridDim.y = unsigned int(ceil(double(GRIDSIZESKYY) / double(blockDim.y)));
	// Error Check
	if (gridDim.x * gridDim.y >= totalBlocksPerGrid)
	{
		printf("Error: current simulation size is greater than available theads and blocks on the x and y axis, use a small simulation size!\n");
		return;
	}
	// Depth is determined by how many blocks we have in total and use per z slice.
	gridDim.z = unsigned int(std::min(double(GRIDSIZESKYZ), floor(double(totalBlocksPerGrid) / double(gridDim.x * gridDim.y))));

	canFillAll = GRIDSIZESKYZ <= gridDim.z;

	// Fill info
	const float _invBlockSpreadDepth = 1.0f / (float(gridDim.z) / float(GRIDSIZESKYZ));
	cudaMemcpyToSymbol(invBlockSpreadDepth, &_invBlockSpreadDepth, sizeof(float), 0, cudaMemcpyHostToDevice);
	simKernelInfo.neighbourData = m_neighbourData;

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Simulation size x: %i, y: %i, z: %i\n", GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Max Blocks: %i, Max Threads: %i, Max Register Per Thread: %i\n", totalBlocksPerGrid, totalThreadsPerBlock, totalRegsPerThread);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Block Dimension x: %i y: %i\n", blockDim.x, blockDim.y);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Blocks on x axis: %i, Blocks on y axis: %i, Blocks on z axis: %i\n", gridDim.x, gridDim.y, gridDim.z);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("GPU: %s\n", prop.name);


	// Malloc space for the pointers
	// Current allocated space:
	//const int totalAllocatedSpace = ((26 * GRIDSIZESKY) + (11 * GRIDSIZEGROUND) + (7 * GRIDSIZESKYY) + 5) * sizeof(float);

	size_t freeMem, totalMem;
	cudaMemGetInfo(&freeMem, &totalMem);
	
	// Environment Values
	cudaMalloc((void**)&m_envGrid.Qv, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.Qw, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.Qc, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.Qr, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.Qs, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.Qi, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.potTemp, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.velfieldX, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.velfieldY, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.velfieldZ, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_envGrid.pressure, GRIDSIZESKY * sizeof(float));

	// Ground values
	cudaMalloc((void**)&m_groundGrid.Qrs, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_groundGrid.Qgr, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_groundGrid.Qgs, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_groundGrid.Qgi, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_groundGrid.P, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_groundGrid.t, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_groundGrid.T, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_dummyArrayGround, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc((void**)&m_dummyArrayGround2, GRIDSIZEGROUND * sizeof(float));

	cudaMalloc((void**)&m_GHeight, GRIDSIZEGROUND * sizeof(int));
	cudaMalloc((void**)&m_dummyGHeight, GRIDSIZEGROUND * sizeof(int));

	cudaMalloc((void**)&m_array, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_outputArray, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_defaultVal, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_neighbourData, GRIDSIZESKY * sizeof(Neigh));
	cudaMalloc((void**)&m_density, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_densityAir, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_oldDensityAir, GRIDSIZESKY * sizeof(float));

	cudaMalloc((void**)&m_microPhysRes, sizeof(microPhysicsParams));

	//Default values
	cudaMalloc((void**)&m_defaultPressure, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_defaultVelX, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_defaultVelZ, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_isentropicTemp, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_isentropicVapor, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_dummyArray, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_dummyArraySky2, GRIDSIZESKYY * sizeof(float));

	//Heat
	cudaMalloc((void**)&m_condens, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_depos, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_freeze, GRIDSIZESKY * sizeof(float));

	//Single values
	cudaMalloc((void**)&m_singleStor0, sizeof(float));
	cudaMalloc((void**)&m_sigma0, sizeof(float));
	cudaMalloc((void**)&m_sigma1, sizeof(float));
	cudaMalloc((void**)&m_firstValid, sizeof(int));
	cudaMalloc((void**)&m_storBool, sizeof(bool));

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Extra storage
	cudaMalloc((void**)&m_stor0, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_stor1, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_stor2, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_A, GRIDSIZESKY * sizeof(float4));
	cudaMalloc((void**)&m_precon, GRIDSIZESKY * sizeof(float));

	cudaMemset(m_envGrid.Qv, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.Qw, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.Qc, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.Qr, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.Qs, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.Qi, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.potTemp, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.velfieldX, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.velfieldY, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_envGrid.velfieldZ, 0, GRIDSIZESKY * sizeof(float));

	setToValue << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.Qrs, 0.001f, 1);
	cudaMemset(m_groundGrid.Qgr, 0, GRIDSIZEGROUND * sizeof(float));
	cudaMemset(m_groundGrid.Qgs, 0, GRIDSIZEGROUND * sizeof(float));
	cudaMemset(m_groundGrid.Qgi, 0, GRIDSIZEGROUND * sizeof(float));
	setToValue << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.P, 1000.0f, 1);
	setToValue << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.T, 315.15f, 1);
	cudaMemset(m_groundGrid.t, 0, GRIDSIZEGROUND * sizeof(float));

	// Check how much memory we used
	size_t freeMem2;
	cudaMemGetInfo(&freeMem2, &totalMem);
	size_t memUsed = (freeMem / 1024 / 1024) - (freeMem2 / 1024 / 1024);

	printf("GPU Memory used: %zu\n", memUsed);

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

environmentGPU::~environmentGPU()
{
	//Free space
	cudaFree(m_precon);
	cudaFree(m_A);
	cudaFree(m_stor2);
	cudaFree(m_stor1);
	cudaFree(m_stor0);

	cudaFree(m_storBool);
	cudaFree(m_firstValid);
	cudaFree(m_sigma1);
	cudaFree(m_sigma0);
	cudaFree(m_singleStor0);

	cudaFree(m_freeze);
	cudaFree(m_depos);
	cudaFree(m_condens);

	cudaFree(m_dummyArraySky2);
	cudaFree(m_dummyArray);
	cudaFree(m_isentropicVapor);
	cudaFree(m_isentropicTemp);
	cudaFree(m_defaultVelZ);
	cudaFree(m_defaultVelX);
	cudaFree(m_defaultPressure);

	cudaFree(m_oldDensityAir);
	cudaFree(m_densityAir);
	cudaFree(m_microPhysRes);
	cudaFree(m_density);
	cudaFree(m_neighbourData);
	cudaFree(m_defaultVal);
	cudaFree(m_outputArray);
	cudaFree(m_array);

	cudaFree(m_dummyGHeight);
	cudaFree(m_GHeight);

	cudaFree(m_dummyArrayGround2);
	cudaFree(m_dummyArrayGround);
	cudaFree(m_groundGrid.T);
	cudaFree(m_groundGrid.t);
	cudaFree(m_groundGrid.P);
	cudaFree(m_groundGrid.Qgi);
	cudaFree(m_groundGrid.Qgs);
	cudaFree(m_groundGrid.Qgr);
	cudaFree(m_groundGrid.Qrs);

	cudaFree(m_envGrid.pressure);
	cudaFree(m_envGrid.velfieldZ);
	cudaFree(m_envGrid.velfieldY);
	cudaFree(m_envGrid.velfieldX);
	cudaFree(m_envGrid.potTemp);
	cudaFree(m_envGrid.Qi);
	cudaFree(m_envGrid.Qs);
	cudaFree(m_envGrid.Qr);
	cudaFree(m_envGrid.Qc);
	cudaFree(m_envGrid.Qw);
	cudaFree(m_envGrid.Qv);
}

void environmentGPU::init(float* potTemps, glm::vec3* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures, float* smallPressure)
{
	//velField[getIdx(31, 16, 16)].y = 10.0f;

	//Init sky
	cudaMemcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	
	//First check
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	{
		float* velX = new float[GRIDSIZESKY];
		float* velY = new float[GRIDSIZESKY];
		float* velZ = new float[GRIDSIZESKY];

		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			velX[i] = velField[i].x;
			velY[i] = velField[i].y;
			velZ[i] = velField[i].z;
		}
		cudaMemcpy(m_envGrid.velfieldX, velX, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
		cudaMemcpy(m_envGrid.velfieldY, velY, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
		cudaMemcpy(m_envGrid.velfieldZ, velZ, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);

		delete[] velX;
		delete[] velY;
		delete[] velZ;
	}

	cudaMemcpy(m_envGrid.Qv, Qv, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);

	cudaMemcpy(m_groundGrid.T, groundTemp, GRIDSIZEGROUND * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(groundTemp, m_groundGrid.T, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);

	cudaMemcpy(m_groundGrid.P, groundPres, GRIDSIZEGROUND * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(m_defaultPressure, smallPressure, GRIDSIZESKYY * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(m_envGrid.pressure, pressures, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);

	//Initialize density
	initDensity<<<GRIDSIZESKYY, GRIDSIZESKYX >>>(m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
	cudaMemcpy(m_oldDensityAir, m_densityAir, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);

	
	//Set values using offset, with the 2D, you can add an offset
	// This also works for 3D, since we first store the X then Y then Z, so we can just ignore the Z.
	cudaMemcpy2D(m_isentropicTemp, sizeof(float), m_envGrid.potTemp, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	cudaMemcpy2D(m_isentropicVapor, sizeof(float), m_envGrid.Qv, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	cudaMemcpy2D(m_defaultVelX, sizeof(float), m_envGrid.velfieldX, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	cudaMemcpy2D(m_defaultVelZ, sizeof(float), m_envGrid.velfieldY, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	cudaMemcpy2D(m_defaultVelZ, sizeof(float), m_envGrid.velfieldZ, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	setToValue << <GRIDSIZESKYY, 1 >> > (m_dummyArray, 1.0f, 1);
	cudaDeviceSynchronize();

	// Randomize the environment a bit
	randomArray << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.potTemp, -0.1f, 0.1f, GRIDSIZESKYZ, 100);
	randomArray << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qv, 0.0f, 0.0001f, GRIDSIZESKYZ, 101);

	randomArray << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldX, 0.5f, 0.5f, GRIDSIZESKYZ, 102);
	randomArray << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldY, -0.2f, 0.2f, GRIDSIZESKYZ, 103);
	randomArray << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldZ, 0.5f, 0.5f, GRIDSIZESKYZ, 104);

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Init ground
	float* noise = nullptr;
	float* noiseGPU = nullptr;
	cudaMallocHost(&noise, GRIDSIZEGROUND * sizeof(float));
	cudaMalloc(&noiseGPU, GRIDSIZEGROUND * sizeof(float));
	//Generate noise
	PNoise2D(100, noise, GRIDSIZESKYX, GRIDSIZESKYZ, 12);
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
	cudaMemcpy(noiseGPU, noise, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDefault);
	const float maxHeight = 0.2f;
	initGroundHeightGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_GHeight, noiseGPU, maxHeight);
	cudaDeviceSynchronize();

	cudaDeviceSynchronize();
	cudaFree(noiseGPU);
	cudaFreeHost(noise);

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Init Editor
	Game.Editor().setIsentropics(m_isentropicTemp, m_isentropicVapor, m_defaultPressure);
	Game.Editor().setTime(m_time);
	Game.Editor().setLongitude(m_longitude);
	Game.Editor().setDay(m_day);

	//Reset values that are in ground
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.potTemp, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qv, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qw, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qc, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qr, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qs, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qi, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldX, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldY, m_GHeight);
	resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldZ, m_GHeight);
	cudaDeviceSynchronize();
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	computeNeighbourGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_neighbourData);
	cudaDeviceSynchronize();
	computeIsenTempGroundGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.T, m_isentropicTemp, m_groundGrid.P, m_envGrid.pressure, m_GHeight);
	cudaDeviceSynchronize();


	//Init other classes
	initKernelSky(m_GHeight, m_defaultVelX, m_defaultVelZ);
	initGammasMicroPhysics();
	//Set editor data
	Game.Editor().GPUSetEnv(&m_envGrid, &m_groundGrid, m_GHeight, m_envGrid.pressure);

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::updateGPU(const float dt, const float speed)
{
	//Main function that goes through the loop
	cudaError_t err = cudaGetLastError();

	// Set values info
	cudaMemcpyToSymbol(simDeltaTime, &dt, sizeof(float));
	cudaMemcpyToSymbol(simSpeed, &speed, sizeof(float));

	setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);

	editorDataGPU();
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	// 1. Update total incoming solar radiation 
	// 2. Update Ground
	// 3. Update Sky

	// 1.
	const float irridiance = irridianceGPU();
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}



	// 2. ---------GROUND----------
	{
		// 1. Advect microphysics ground
		// 2. Compute Cloud Covering Fraction 
		// 3. Update ground temperature
		// 4. Update microphysic process ground and update ground temp, also precip hitting ground

		// 1.
		advectGroundWater(dt, speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
		// 2.
		groundCoverageFactor();
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		// 3.
		updateGroundTemps(dt, speed, irridiance);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		// 4.
		microPhysicsGroundGPU(dt, speed, irridiance);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
	}

	// 3. -----------SKY-----------
	{
		// 1. Diffuse and Advect Potential Temp
		
		// 2. Add Forces, Diffuse, Advect and Pressure Project Velocity Field
		
		// 3. Diffuse and Advect water content of Qj
		
		// 4. Update microphysics and Compute heat transfer 

		// 1. 
		setTempsAtGround(dt, speed);
		diffuseGPU(m_envGrid.potTemp, 0, dt * speed);
		advectPPMWGPU(m_envGrid.potTemp, m_isentropicTemp, m_boundsPotTemp, dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}


		 //2.

		calculateBuoyancy(dt * speed);
		diffuseGPU(m_envGrid.velfieldX, 2, dt * speed);
		diffuseGPU(m_envGrid.velfieldY, 3, dt * speed);
		diffuseGPU(m_envGrid.velfieldZ, 4, dt * speed);
		advectPPMWGPU(m_envGrid.velfieldX, m_defaultVelX, m_boundsVelX, dt * speed);
		cudaMemset(m_dummyArray, 0, GRIDSIZESKYY * sizeof(float));
		advectPPMWGPU(m_envGrid.velfieldY, m_dummyArray, m_boundsVelY, dt * speed);
		advectPPMWGPU(m_envGrid.velfieldZ, m_defaultVelZ, m_boundsVelZ, dt * speed);

		initDensity << <GRIDSIZESKYY, GRIDSIZESKYX>> > (m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
		//cudaDeviceSynchronize();
		cudaMemcpy(m_oldDensityAir, m_densityAir, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
		
		pressureProject(dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
		
		calculateNewPressure << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.pressure, m_densityAir, m_envGrid.potTemp, m_envGrid.Qv, m_groundGrid.P);
		cudaDeviceSynchronize();




		// 3.	
		diffuseGPU(m_envGrid.Qv, 1, dt * speed);//Vapor
		advectPPMWGPU(m_envGrid.Qv, m_isentropicVapor, m_boundsVapor, dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {

			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		diffuseGPU(m_envGrid.Qw, 5, dt * speed);//Water cloud
		advectPPMWGPU(m_envGrid.Qw, m_dummyArray, m_boundsMixingRatios, dt* speed);
		
		diffuseGPU(m_envGrid.Qc, 5, dt * speed);//Ice cloud
		advectPPMWGPU(m_envGrid.Qc, m_dummyArray, m_boundsMixingRatios, dt * speed);
		
		diffuseGPU(m_envGrid.Qr, 5, dt * speed);//Rain
		advectPrecip(m_envGrid.Qr, 0, dt * speed);
		advectPPMWGPU(m_envGrid.Qr, m_dummyArray, m_boundsMixingRatios, dt * speed);
		
		diffuseGPU(m_envGrid.Qs, 5, dt* speed);//Snow
		advectPrecip(m_envGrid.Qs, 1, dt* speed);
		advectPPMWGPU(m_envGrid.Qs, m_dummyArray, m_boundsMixingRatios, dt* speed);
		
		diffuseGPU(m_envGrid.Qi, 5, dt * speed);//Ice
		advectPrecip(m_envGrid.Qi, 2, dt * speed);
		advectPPMWGPU(m_envGrid.Qi, m_dummyArray, m_boundsMixingRatios, dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		// 4.	
		microPhysicsSkyGPU(dt, speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
	}

	cudaDeviceSynchronize();

	//Set editor data
	m_time += speed * dt * float(!m_pauseDiurnal);
	if (m_time > 86400.0f) m_time = 0.0f;

	Game.Editor().GPUSetEnv(&m_envGrid, &m_groundGrid, m_GHeight, m_envGrid.pressure);
	m_groundChanged = false;
}

void environmentGPU::microPhysicsGroundGPU(const float dt, const float speed, const float irradiance)
{
	calculatePrecipHittingGroundMicroPhysicsGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		m_groundGrid.Qgr, m_groundGrid.Qgs, m_groundGrid.Qgi, dt, speed, m_groundGrid.T, m_envGrid.potTemp,
		m_densityAir, m_envGrid.pressure, m_groundGrid.P, m_envGrid.velfieldX, m_GHeight);

	calculateGroundMicroPhysicsGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.Qrs, m_envGrid.Qv, m_groundGrid.Qgr, m_groundGrid.Qgs, m_groundGrid.Qgi,
		dt, speed, m_groundGrid.T, m_envGrid.potTemp, m_densityAir, m_envGrid.pressure, m_groundGrid.P, m_groundGrid.t, irradiance, m_envGrid.velfieldX, m_dummyArrayGround, m_GHeight);
	//cudaDeviceSynchronize();
}

void environmentGPU::microPhysicsSkyGPU(const float dt, const float speed)
{
	//Reset
	cudaMemset(m_microPhysRes, 0, sizeof(microPhysicsParams));

	calculateEnvMicroPhysicsGPU<< <GRIDSIZESKYY, GRIDSIZESKYX >> >(m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		dt, speed, m_envGrid.potTemp, m_densityAir, m_envGrid.pressure, m_GHeight, m_groundGrid.P,
		m_condens, m_depos, m_freeze, Game.DataClass().microPhysCheckActive, Game.DataClass().microPhysMinPos, Game.DataClass().microPhysMaxPos, *m_microPhysRes);
	//cudaDeviceSynchronize();

	//Add data to the data class
	if (Game.DataClass().microPhysCheckActive)
	{
		Game.DataClass().setMicroPhysicsData(m_microPhysRes);
	}
	//cudaDeviceSynchronize();


	//Compute heat and add it to the potTemp
	addHeatGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qv, m_envGrid.potTemp, m_condens, m_depos, m_freeze);
	//cudaDeviceSynchronize();

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::diffuseGPU(float* diffuseArray, int type, const float dt)
{
	float* defaultArray = nullptr;
	boundsEnv boundsConditions{ NEUMANN, DIRICHLET, DIRICHLET };

	switch (type)
	{
	case 0: //Temp
		defaultArray = m_isentropicTemp;
		// Sides boundaries are not actually dirichlet, but we customize inside the function for temp
		boundsConditions.ground = CUSTOM;
		boundsConditions.sides = CUSTOM;
		boundsConditions.up = CUSTOM;
		break;
	case 1: //Vapor
		defaultArray = m_isentropicVapor;
		boundsConditions.ground = DIRICHLET;
		boundsConditions.sides = CUSTOM;
		boundsConditions.up = CUSTOM;
		break;
	case 2: //Vel-X
		defaultArray = m_defaultVelX;
		boundsConditions.sides = CUSTOM;
		boundsConditions.up = CUSTOM;
		break;
	case 3: //Vel-Y
		defaultArray = nullptr; //Not going to even use this
		boundsConditions.sides = NEUMANN;
		boundsConditions.up = DIRICHLET;
		break;
	case 4: //Vel-Z
		defaultArray = m_defaultVelZ; //Not going to even use this
		boundsConditions.sides = CUSTOM;
		boundsConditions.up = CUSTOM;
		break;
	case 5: //Default
		cudaMemset(m_dummyArray, 0, GRIDSIZESKYY * sizeof(float));
		defaultArray = m_dummyArray;
		break;
	}
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	cudaMemcpy(m_array, diffuseArray, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
	const float k = 0.005f * dt / (VOXELSIZE * VOXELSIZE); //Viscosity value
	const int LOOPS = 20 / 2; //Total loops for the Gauss-Seidel method (divided by 2 due to doing 2 times)

	const int sharedDataSize = (blockDim.x + 2) * (blockDim.y + 2) * sizeof(float);


	//Create events to track time taking of parts
	//cudaEvent_t start;
	//cudaEvent_t stop;
	//cudaEventCreate(&start);
	//cudaEventCreate(&stop);
	//cudaEventRecord(start);

	for (int L = 0; L < LOOPS; L++)
	{
		diffuseRedBlack << <gridDim, blockDim, sharedDataSize >> > (m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_array, m_outputArray, k, type, boundsConditions, true, m_neighbourData);
		//cudaDeviceSynchronize();
		diffuseRedBlack << <gridDim, blockDim, sharedDataSize >> > (m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_array, m_outputArray, k, type, boundsConditions, false, m_neighbourData);
		//cudaDeviceSynchronize();

		//Switch output and input around
		diffuseRedBlack << <gridDim, blockDim, sharedDataSize >> > (m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_outputArray, m_array, k, type, boundsConditions, true, m_neighbourData);
		//cudaDeviceSynchronize();
		diffuseRedBlack << <gridDim, blockDim, sharedDataSize >> > (m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_outputArray, m_array, k, type, boundsConditions, false, m_neighbourData);
		//cudaDeviceSynchronize();
	}
	//cudaDeviceSynchronize();
	cudaMemcpy(diffuseArray, m_array, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
	////End recording and record time
	//cudaEventRecord(stop);
	//cudaStreamSynchronize(0);
	//float elapsedTime;
	//cudaEventElapsedTime(&elapsedTime, start, stop);
	//std::cout << "Execution time: " << elapsedTime << " ms" << std::endl;
	//cudaEventDestroy(start);
	//cudaEventDestroy(stop);

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, diffuseRedBlack);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);


	err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

}

void environmentGPU::advectGroundWater(const float dt, const float speed)
{
	// For the ground we use also use block and gridsize, but now X and Z instead of Y
	dim3 grid(std::min(16, GRIDSIZESKYX), std::min(16, GRIDSIZESKYZ));
	dim3 block((GRIDSIZESKYX + 15) / 16, (GRIDSIZESKYZ + 15) / 16);

	//Need red-black due to accessing previous index
	advectGroundWaterGPU<<<grid, block>>> (m_groundGrid.Qrs, m_groundGrid.Qgr);
	//cudaDeviceSynchronize();
}

void environmentGPU::setTempsAtGround(const float dt, const float speed)
{
	setTempsAtGroundGPU<<<GRIDSIZESKYZ, GRIDSIZESKYX >>>(m_envGrid.potTemp, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, dt * speed);
	cudaDeviceSynchronize();
}

void environmentGPU::advectPPMWGPU(float* advectArray, const float* defaultVal, boundsEnv boundsVal, const float dt)
{
	//Create events to track time taking of parts
	//cudaEvent_t start;
	//cudaEvent_t stop;
	//cudaEventCreate(&start);
	//cudaEventCreate(&stop);
	//cudaEventRecord(start);


	//Reset data
	cudaMemset(m_outputArray, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_stor0, 0, GRIDSIZESKY * sizeof(float));
	setToValue << <1, GRIDSIZESKYY >> > (m_dummyArraySky2, 1.0f, 1);

	//Recollect all threads before starting kernels
	//cudaDeviceSynchronize();


	// Substeps based on highest courant number
	// This is to make sure that the advecting value can not pass over 1 cell, for example:
	// If velocity is too high, the value actually never passes this cell in one time frame but passes over.
	// To fix this we use sub-steps.
	cudaMemset(m_singleStor0, 0, sizeof(float));
	const int sharedDataSizeMaxDiv = blockDim.x * blockDim.y * sizeof(float);

	// Get max velocity and base C of this.
	getMaxDivergence << <gridDim, blockDim, sharedDataSizeMaxDiv >> > (m_singleStor0, m_envGrid.velfieldX);
	getMaxDivergence << <gridDim, blockDim, sharedDataSizeMaxDiv >> > (m_singleStor0, m_envGrid.velfieldY);
	getMaxDivergence << <gridDim, blockDim, sharedDataSizeMaxDiv >> > (m_singleStor0, m_envGrid.velfieldZ);
	float maxVel = 0.0f;
	cudaMemcpy(&maxVel, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost); // Causing long stall due to GPU work catching up to this point.

	const float C = maxVel * dt / VOXELSIZE;
	const float MAXSUBSTEPS = 100;

	const int subSteps = int(fminf(ceilf(fabsf(C)), MAXSUBSTEPS));
	const float dtSub = dt / subSteps;
	if (C > 25)
	{
		printf("WARNING: Courant number is high: %f, increase size of voxels or decrease timesteps, %f, %i\n", C, dtSub, subSteps);
	}

	const int sharedDataSize = (blockDim.x + 2) * (blockDim.y + 2) * sizeof(float);


	// We advect the default value and density. 
	// After each time we advect, we need to divide by the density, this is to make sure any errors that accumulate are immediately resolved.
	// Using half x, half y and full z, we use second order accuracy, making sure we treat x, y and z equally. 
	for (int i = 0; i < 1; i++)
	{
		// Using strang method
		// Also used in flash https://flash.rochester.edu/site/flashcode/user_support/flash2_users_guide/docs/FLASH2.5/flash2_ug.pdf
		// in 6.1.3, strang is done using X, Y, Z, then another timestep for Z, Y, X. Combining that in 1 timestep, we can do:
		// 0.5X, 0.5Y, Z, 0.5Y, 0.5X
		

		// Its kernel time, advecting 2 times half X, 2 times half Y and 1 time Z
		
		//------------------------------ 0.5 X -----------------------------------

		//First advect density and array on half X
		advectPPMX << <gridDim, blockDim, sharedDataSize >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldX, m_neighbourData, boundsVal, dtSub * 0.5f);
		advectPPMX << <gridDim, blockDim, sharedDataSize >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, boundsVal, dtSub * 0.5f);
		//cudaDeviceSynchronize();

		//Divide to get them both up to speed
		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		//cudaDeviceSynchronize();

		//------------------------------ 0.5 Y -----------------------------------

		//Switch blocks and threads around (x and y are swapped)
		//Also input and output are swapped because of previous result
		advectPPMY << <gridDim, blockDim, sharedDataSize >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldY, m_neighbourData, boundsVal, dtSub * 0.5f);
		advectPPMY << <gridDim, blockDim, sharedDataSize >> > (m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, boundsVal, dtSub * 0.5f);
		//cudaDeviceSynchronize();

		//Divide to get them both up to speed
		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (advectArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		//cudaDeviceSynchronize();

		//------------------------------ 1.0 Z -----------------------------------

		advectPPMZ << <gridDim, blockDim >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldZ, m_neighbourData, boundsVal, dtSub);
		advectPPMZ << <gridDim, blockDim >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldZ, m_neighbourData, boundsVal, dtSub);
		//cudaDeviceSynchronize();

		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		//cudaDeviceSynchronize();

		//------------------------------ 0.5 Y -----------------------------------

		advectPPMY << <gridDim, blockDim, sharedDataSize >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldY, m_neighbourData, boundsVal, dtSub * 0.5f);
		advectPPMY << <gridDim, blockDim, sharedDataSize >> > (m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, boundsVal, dtSub * 0.5f);
		//cudaDeviceSynchronize();

		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (advectArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		//cudaDeviceSynchronize();

		//------------------------------ 0.5 X -----------------------------------

		advectPPMX << <gridDim, blockDim, sharedDataSize >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldX, m_neighbourData, boundsVal, dtSub * 0.5f);
		advectPPMX << <gridDim, blockDim, sharedDataSize >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, boundsVal, dtSub * 0.5f);
		//cudaDeviceSynchronize();

		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);

		//Switch over input and output for our next iteration or return result.
		cudaMemcpy(advectArray, m_outputArray, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
	}
		//cudaDeviceSynchronize();

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, advectPPMZ);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
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

void environmentGPU::advectPrecip(float* array, const int fallVelType, const float dt)
{
	// Substeps based on highest courant number
	// This is to make sure that the advecting value can not pass over 1 cell, for example:
	// If velocity is too high, the value actually never passes this cell in one time frame but passes over.
	// To fix this we use sub-steps.

	//Guesses since we don't have access to speeds
	float maxVel = fallVelType == 0 ? 5.0f : (fallVelType == 1 ? 1.0f : 8.0f);


	const float C = maxVel * dt / VOXELSIZE;
	const float MAXSUBSTEPS = 100;
	const int subSteps = int(fminf(ceilf(fabsf(C)), MAXSUBSTEPS));
	const float dtSub = dt / subSteps;

	//Advect precip for the substeps.
	for (int i = 0; i < subSteps; i++)
	{
		// Psst, we swapped the X and Y around since we are only interested in the Y values per block
		advectPrecipGPU<<< GRIDSIZESKYX, GRIDSIZESKYY>>>(array, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.pressure, m_groundGrid.P, fallVelType, dtSub);
		//cudaDeviceSynchronize();
	}
}

void environmentGPU::pressureProject(const float dt)
{
	boundsEnv boundsDensity{ NEUMANN, NEUMANN, DIRICHLET };
	boundsEnv boundsA{ NEUMANN, DIRICHLET, DIRICHLET };

	const int sharedDataSize = (blockDim.x + 2) * (blockDim.y + 2) * sizeof(float);

	setToValue << <1, GRIDSIZESKYY >> > (m_dummyArraySky2, 1.0f, 1);

	//Should not be needed if handled correctly in advection and other velocity updates?
	resetVelPressProj << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ);
	//cudaDeviceSynchronize();

	//Initialize A and precon every frame due to changed based on density
	initAMatrix << <gridDim, blockDim, sharedDataSize >> > (m_A, m_neighbourData, m_densityAir, m_dummyArraySky2, boundsA);
	//cudaDeviceSynchronize();

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, initAMatrix);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}


	initPrecon << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_precon, m_A);
	//cudaDeviceSynchronize();

	//Actual pressure project
	calculatePressureProject(m_outputArray, dt);


	//Set new density
	updatePressure << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.pressure, m_outputArray);
	initDensity << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
	//cudaDeviceSynchronize();

	//Debug
	Game.Editor().setDebugValueNum(m_outputArray, 2);
	//cudaDeviceSynchronize();

	//Apply calculate pressure to velocity field
	applyPresProjGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ,
		m_densityAir, m_envGrid.pressure, boundsDensity, dt);
	//cudaDeviceSynchronize();

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::calculatePressureProject(float* outputPressure, const float )
{	
	boundsEnv boundsDensity{ NEUMANN, NEUMANN, DIRICHLET };
	boundsEnv boundsA{ DIRICHLET, DIRICHLET, DIRICHLET };

	const float tolValue = 1e-5f;
	const int MAXITERATION = 200;
	float maxr = 0.0f;
	const int sharedDataSize = (blockDim.x + 2) * (blockDim.y + 2) * sizeof(float);
	const int sharedDataSizeNoHalo = blockDim.x * blockDim.y * sizeof(float);

	//m_stor0 = divergence
	//m_stor1 = z
	//m_stor2 = s
	//Reset values
	cudaMemset(m_stor0, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_stor1, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_stor2, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(outputPressure, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_singleStor0, 0, sizeof(float));
	cudaMemset(m_sigma0, 0, sizeof(float));
	cudaMemset(m_sigma1, 0, sizeof(float));


	//Divergence
	calculateDivergenceGPU << <gridDim, blockDim, sharedDataSize >> > (m_stor0, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_densityAir, m_oldDensityAir, m_dummyArraySky2, boundsDensity, m_boundsVelX);
	//cudaDeviceSynchronize();

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, calculateDivergenceGPU);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "CUDA error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Debug
	Game.Editor().setDebugValueNum(m_stor0, 2);
	//cudaDeviceSynchronize();

	//Check max divergence
	getMaxDivergence << <gridDim, blockDim, sharedDataSizeNoHalo >> > (m_singleStor0, m_stor0);
	//cudaDeviceSynchronize();
	cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
	if (maxr == 0.0f) return;

	applyPreconditionerGPU << <gridDim, blockDim >> > (m_stor1, m_precon, m_stor0, m_A);
	//cudaDeviceSynchronize();


	cudaMemcpy(m_stor2, m_stor1, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
	dotProductGPU << <gridDim, blockDim, sharedDataSizeNoHalo >> > (m_sigma0, m_stor1, m_stor0);
	//cudaDeviceSynchronize();

	for (int i = 0; i < MAXITERATION; i++)
	{
		// Same bounds as density since A = density
		applyAGPU<<<gridDim, blockDim, sharedDataSize >>>(m_stor1, m_stor2, m_neighbourData, m_A, boundsA);
		//cudaDeviceSynchronize();

		dotProductGPU << <gridDim, blockDim, sharedDataSizeNoHalo >> > (m_sigma1, m_stor1, m_stor2);
		//cudaDeviceSynchronize();

		//Update the pressure (output) and divergence
		updatePandDiv << <gridDim, blockDim >> > (m_sigma0, m_sigma1, outputPressure, m_stor0, m_stor2, m_stor1);
		//cudaDeviceSynchronize();

		//Check max divergence
		cudaMemset(m_singleStor0, 0, sizeof(float));
		getMaxDivergence << <gridDim, blockDim, sharedDataSizeNoHalo >> > (m_singleStor0, m_stor0);
		//cudaDeviceSynchronize();
		//cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
		//if (maxr <= tolValue)
		//{
		//	printf("Iterations pressure projection: %i\n", i);
		//	return;
		//}

		applyPreconditionerGPU << <gridDim, blockDim >> > (m_stor1, m_precon, m_stor0, m_A);

		//cudaDeviceSynchronize();

		//Dotproduct
		cudaMemset(m_sigma1, 0, sizeof(float));
		dotProductGPU << <gridDim, blockDim, sharedDataSizeNoHalo >> > (m_sigma1, m_stor1, m_stor0);
		//cudaDeviceSynchronize();

		//Set values and set search vector
		endIteration << <gridDim, blockDim >> > (m_sigma0, m_sigma1, m_stor2, m_stor1);

		cudaMemset(m_sigma1, 0, sizeof(float));

		//cudaDeviceSynchronize();
	}
	//printf("Max iterations reached!\n");
}

void environmentGPU::editorDataGPU()
{
	//Set/Get editor data
	if (m_groundChanged || Game.Editor().changedGround())
	{
		//Reset values that are in ground
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.potTemp, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qv, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qw, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qc, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qr, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qs, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qi, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldX, m_GHeight);
		resetValueInGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldY, m_GHeight);
		cudaDeviceSynchronize();

		computeNeighbourGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_neighbourData);
		computeIsenTempGroundGPU << <GRIDSIZESKYZ, GRIDSIZESKYX>> > (m_groundGrid.T, m_isentropicTemp, m_groundGrid.P, m_envGrid.pressure, m_GHeight);
	}

	m_pauseDiurnal = Game.Editor().getDiurnalCyclePaused();
	m_longitude = Game.Editor().getLongitude();
	m_day = Game.Editor().getDay();
	m_sunStrength = Game.Editor().getSunStrength();
	Game.Editor().getTime(m_time);
	Game.Editor().setTime(m_time); //For if time did not change
}

float environmentGPU::irridianceGPU()
{
	//Calculates the total energy from the sun at a specific spot
	//Using formulas from https://tc.copernicus.org/articles/17/211/2023/
	float Gs = 1361.0f * m_sunStrength; // Solar constant in W/m-2
	float rd = 1 + 0.034f * cosf(2 * 3.14159265359f * m_day / 365); //Relative distance to the sun
	float sd = 0.409f * sinf(2 * 3.14159265359f / 365 * (m_day - 81)); //Solar diclenation with spring equinox on day 81
	const float timeHour = m_time / 3600;
	const float longitudeRad = glm::radians(m_longitude);

	float solarRad = (timeHour - (m_hourOfSunrise + m_dayLightDuration / 2.0f)) * (3.14159265359f / 12.0f); //Convert time to noon to radians.
	//Get amount of W/m-2 at this time of the day.
	return std::max(0.0f, Gs * rd * (sinf(longitudeRad) * sinf(sd) + cosf(longitudeRad) * cosf(sd) * cosf(solarRad)));
}

void environmentGPU::groundCoverageFactor()
{
	calculateCloudCoverGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_dummyArrayGround, m_envGrid.Qc, m_envGrid.Qw, m_GHeight);
	//cudaDeviceSynchronize();
}

void environmentGPU::updateGroundTemps(const float dt, const float speed, const float irridiance)
{
	calculateGroundTempGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.T, dt * speed, irridiance, m_dummyArrayGround);
	//cudaDeviceSynchronize();
}

void environmentGPU::calculateBuoyancy(const float dt)
{
	// Neumann, since we will just use the current temp but divide by 2, meaning we don't actually use outside or ground
	boundsEnv buoyancyBounds{ DIRICHLET, DIRICHLET, DIRICHLET };

	const int sharedDataSize = (blockDim.x + 2) * (blockDim.y + 2) * sizeof(float);

	buoyancyGPU << <gridDim, blockDim, sharedDataSize >> > (m_envGrid.velfieldY, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, m_isentropicTemp, m_isentropicVapor, m_envGrid.pressure, m_groundGrid.P, m_stor0, buoyancyBounds, m_boundsVelY);
	cudaDeviceSynchronize();
	//Set debug
	Game.Editor().setDebugValueNum(m_stor0, 1);

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, buoyancyGPU);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);


	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

bool environmentGPU::isGround(int x, int y)
{
	return y <= m_GHeight[x];
}

float* environmentGPU::getParamArray(parameter type, direction windDir)
{
	switch (type)
	{
	case POTTEMP:
		return m_envGrid.potTemp;
		break;
	case QV:
		return m_envGrid.Qv;
		break;
	case QW:
		return m_envGrid.Qw;
		break;
	case QC:
		return m_envGrid.Qc;
		break;
	case QR:
		return m_envGrid.Qr;
		break;
	case QS:
		return m_envGrid.Qs;
		break;
	case QI:
		return m_envGrid.Qi;
		break;
	case WIND:
		switch (windDir)
		{
		case LEFT:
		case RIGHT:
			return m_envGrid.velfieldX;
		case UP:
		case DOWN:
			return m_envGrid.velfieldY;
		case FORWARD:
		case BACKWARD:
			return m_envGrid.velfieldZ;
		default:
			break;
		}
		break;
	case PGROUND:
		printf("Error, getParamArray() can not return PGROUND, must return float, change return value to template or void to fix\n");
		break;
	default:
		printf("Error, getParamArray() value not supported\n");
		break;
	}
	return nullptr;
}

void environmentGPU::prepareBrushGPU(parameter paramType, const float brushSize, const int3 mousePos, const float brushSmoothnes, const float dt, const float brushIntensity, const float applyValue, const float3 valueDir, const bool groundErase)
{
	cudaMemcpy(m_dummyGHeight, m_GHeight, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToDevice);
	bool changedGround = false;
	cudaMemset(m_storBool, 0, sizeof(bool));

	int blocks = int(std::ceil(brushSize)) - int(std::floor(-brushSize));
	int threads = blocks;

	//Get correct array to possibly change
	float* array = nullptr;
	float* array2 = nullptr;
	float* array3 = nullptr;
	if (paramType == WIND)
	{
		array = getParamArray(paramType, RIGHT);
		array2 = getParamArray(paramType, UP);
		array3 = getParamArray(paramType, FORWARD);
	}
	else if (paramType != PGROUND)
	{
		array = getParamArray(paramType);
	}
	//Actually calculate where to put brush and apply brush values
	applyBrushGPU << <blocks, threads >> > (array, array2, array3, m_dummyGHeight, m_storBool, paramType, brushSize, mousePos, brushSmoothnes, brushIntensity, applyValue, valueDir, groundErase, dt);
	cudaDeviceSynchronize();

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Check for ground changed
	cudaMemcpy(&changedGround, m_storBool, sizeof(bool), cudaMemcpyDeviceToHost);
	if (changedGround)
	{
		//debugPrintArray << <1, threads >> > (m_GHeight, GRIDSIZESKYZ);
		//First set all data back that was in the ground before
		if (groundErase)
		{
			compareAndResetValuesOutGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_GHeight, m_dummyGHeight, m_isentropicTemp, m_isentropicVapor,
				m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, 
				m_envGrid.potTemp, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_envGrid.pressure, m_defaultPressure);
			cudaDeviceSynchronize();
		}
		//Then set the groundheight correct
		cudaMemcpy(m_GHeight, m_dummyGHeight, GRIDSIZEGROUND * sizeof(int), cudaMemcpyDeviceToDevice);
		cudaDeviceSynchronize();

		initKernelSky(m_GHeight, m_defaultVelX, m_defaultVelZ);
		cudaDeviceSynchronize();
		m_groundChanged = true;
	}

	//Update GPU values
	editorDataGPU();
	cudaDeviceSynchronize();

	Game.Editor().GPUSetEnv(&m_envGrid, &m_groundGrid, m_GHeight, m_envGrid.pressure);
	cudaDeviceSynchronize();
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::prepareSelectionGPU(parameter paramType, const int3 minPos, const int3 maxPos, const float applyValue, const float3 valueDir, const bool groundErase)
{
	cudaMemcpy(m_dummyGHeight, m_GHeight, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToDevice);
	bool changedGround = false;
	cudaMemset(m_storBool, 0, sizeof(bool));

	int blocks = maxPos.y - minPos.y + 1;
	int threads = maxPos.x - minPos.x + 1;

	if (blocks <= 0 || threads <= 0)
	{
		printf("Warning: block (%i) or thread (%i) amount are invalid, setting value to 1 \n", blocks, threads);
		blocks = blocks <= 0 ? 1 : blocks;
		threads = threads <= 0 ? 1 : threads;
	}

	//Get correct array to possibly change
	float* array = nullptr;
	float* array2 = nullptr;
	float* array3 = nullptr;
	if (paramType == WIND)
	{
		array = getParamArray(paramType, RIGHT);
		array2 = getParamArray(paramType, UP);
		array3 = getParamArray(paramType, FORWARD);
	}
	else if (paramType != PGROUND)
	{
		array = getParamArray(paramType);
	}
	// Now actually apply the selection to the grid
	applySelectionGPU << <blocks, threads >> > (array, array2, array3, m_dummyGHeight, m_storBool, paramType, minPos, maxPos, applyValue, valueDir, groundErase);

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Check for ground changed
	cudaMemcpy(&changedGround, m_storBool, sizeof(bool), cudaMemcpyDeviceToHost);
	if (changedGround)
	{
		//First set all data back that was in the ground before
		if (groundErase)
		{
			compareAndResetValuesOutGround << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_GHeight, m_dummyGHeight, m_isentropicTemp, m_isentropicVapor,
				m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
				m_envGrid.potTemp, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_envGrid.pressure, m_defaultPressure);
			cudaDeviceSynchronize();
		}
		//Then set the groundheight correct
		cudaMemcpy(m_GHeight, m_dummyGHeight, GRIDSIZEGROUND * sizeof(int), cudaMemcpyDeviceToDevice);
		cudaDeviceSynchronize();

		initKernelSky(m_GHeight, m_defaultVelX, m_defaultVelZ);
		cudaDeviceSynchronize();
		m_groundChanged = true;
	}

	//Update GPU values
	editorDataGPU();
	cudaDeviceSynchronize();

	Game.Editor().GPUSetEnv(&m_envGrid, &m_groundGrid, m_GHeight, m_envGrid.pressure);
	cudaDeviceSynchronize();
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::resetParameterGPU(parameter paramType)
{
	if (paramType == POTTEMP) setToDefault << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.potTemp, m_isentropicTemp);
	if (paramType == WIND)
	{
		setToDefault << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldX, m_defaultVelX);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldY, 0.0f, GRIDSIZESKYZ);
		setToDefault << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.velfieldZ, m_defaultVelZ);
	}
	if (paramType == PRESSURE)
	{
		setToDefault << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.pressure, m_defaultPressure);
	}
	if (paramType == QV) setToDefault << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qv, m_isentropicVapor);
	if (paramType == QW) setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qw, 0.0f, GRIDSIZESKYZ);
	if (paramType == QC) setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qc, 0.0f, GRIDSIZESKYZ);
	if (paramType == QR) setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qr, 0.0f, GRIDSIZESKYZ);
	if (paramType == QS) setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qs, 0.0f, GRIDSIZESKYZ);
	if (paramType == QI) setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qi, 0.0f, GRIDSIZESKYZ);
}

envDebugData* environmentGPU::getDebugData()
{
	return new envDebugData();
};