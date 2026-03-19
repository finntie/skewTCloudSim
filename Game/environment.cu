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

#include "math/meteoformulas.h"
#include "meteoformulas.cuh"
#include "kernelMaths.cuh"
#include "math/geometry.hpp"

#include "editor.h"
#include "game.h"

// Grid and Block size based on size of simulation
constexpr int GRIDSIZEX = ((GRIDSIZESKYX + 15) / 16);
constexpr int GRIDSIZEZ = ((GRIDSIZESKYZ + 15) / 16);
constexpr int GRIDSIZEY = ((GRIDSIZESKYY + 15) / 16);
constexpr int BLOCKSIZEX = 16;
constexpr int BLOCKSIZEY = 16;
constexpr int BLOCKSIZEZ = 16;


environmentGPU::environmentGPU()
{
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


	//Extra storage
	cudaMalloc((void**)&m_stor0, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_stor1, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_stor2, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_A, GRIDSIZESKY * sizeof(char4));
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

	setToValue << <1, GRIDSIZEGROUND >> > (m_groundGrid.Qrs, 0.001f, GRIDSIZEGROUND);
	cudaMemset(m_groundGrid.Qgr, 0, GRIDSIZEGROUND * sizeof(float));
	cudaMemset(m_groundGrid.Qgs, 0, GRIDSIZEGROUND * sizeof(float));
	cudaMemset(m_groundGrid.Qgi, 0, GRIDSIZEGROUND * sizeof(float));
	setToValue << <1, GRIDSIZEGROUND >> > (m_groundGrid.P, 1000.0f, GRIDSIZEGROUND);
	setToValue << <1, GRIDSIZEGROUND >> > (m_groundGrid.T, 315.15f, GRIDSIZEGROUND);
	cudaMemset(m_groundGrid.t, 0, GRIDSIZEGROUND * sizeof(float));

	size_t freeMem2;
	cudaMemGetInfo(&freeMem2, &totalMem);
	size_t memUsed = (freeMem / 1024 / 1024) - (freeMem2 / 1024 / 1024);

	printf("GPU Memory used: %zu\n",memUsed);

	cudaDeviceProp prop;
	cudaGetDeviceProperties(&prop, 0);
	printf("Max blocks: %i, Max Threads: %i\n", prop.maxBlocksPerMultiProcessor * prop.multiProcessorCount, prop.maxThreadsPerBlock);
	printf("GPU: %s\n", prop.name);
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
	//Init sky
	cudaMemcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	
	//First check
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	cudaMemcpy(potTemps, m_envGrid.potTemp, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);

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
	cudaMemcpy2D(m_defaultVelZ, sizeof(float), m_envGrid.velfieldZ, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	setToValue << <GRIDSIZESKYY, 1 >> > (m_dummyArray, 1.0f, 1);
	cudaDeviceSynchronize();

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

	cudaMemcpy(noiseGPU, noise, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDefault);
	const float maxHeight = 0;
	initGroundHeightGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_GHeight, noiseGPU, maxHeight);
	cudaDeviceSynchronize();
	cudaFree(noiseGPU);
	cudaFree(noise);

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
		advectPPMWGPU(m_envGrid.potTemp, m_isentropicTemp, dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}


		// 2.

		calculateBuoyancy(dt * speed);
		diffuseGPU(m_envGrid.velfieldX, 2, dt * speed);
		diffuseGPU(m_envGrid.velfieldY, 3, dt * speed);
		advectPPMWGPU(m_envGrid.velfieldX, m_defaultVelX, dt * speed);
		cudaMemset(m_dummyArray, 0, GRIDSIZESKYY * sizeof(float));
		advectPPMWGPU(m_envGrid.velfieldY, m_dummyArray, dt * speed);
		advectPPMWGPU(m_envGrid.velfieldZ, m_defaultVelZ, dt * speed);
		initDensity << <GRIDSIZESKYY, GRIDSIZESKYX>> > (m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
		cudaDeviceSynchronize();
		cudaMemcpy(m_oldDensityAir, m_densityAir, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);

		pressureProject(dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
		//Game.Editor().setDebugValueNum(m_densityAir, 2);

		calculateNewPressure << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.pressure, m_densityAir, m_envGrid.potTemp, m_envGrid.Qv, m_groundGrid.P);
		cudaDeviceSynchronize();




		// 3.	
		diffuseGPU(m_envGrid.Qv, 1, dt * speed);//Vapor
		advectPPMWGPU(m_envGrid.Qv, m_isentropicVapor, dt * speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {

			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		diffuseGPU(m_envGrid.Qw, 4, dt * speed);//Water cloud
		advectPPMWGPU(m_envGrid.Qw, m_dummyArray, dt* speed);

		diffuseGPU(m_envGrid.Qc, 4, dt * speed);//Ice cloud
		advectPPMWGPU(m_envGrid.Qc, m_dummyArray, dt * speed);
		
		diffuseGPU(m_envGrid.Qr, 4, dt * speed);//Rain
		advectPrecip(m_envGrid.Qr, 0, dt * speed);
		advectPPMWGPU(m_envGrid.Qr, m_dummyArray, dt * speed);

		diffuseGPU(m_envGrid.Qs, 4, dt* speed);//Snow
		advectPrecip(m_envGrid.Qs, 1, dt* speed);
		advectPPMWGPU(m_envGrid.Qs, m_dummyArray, dt* speed);

		diffuseGPU(m_envGrid.Qi, 4, dt * speed);//Ice
		advectPrecip(m_envGrid.Qi, 2, dt * speed);
		advectPPMWGPU(m_envGrid.Qi, m_dummyArray, dt * speed);
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
	cudaDeviceSynchronize();
}

void environmentGPU::microPhysicsSkyGPU(const float dt, const float speed)
{
	//Reset
	cudaMemset(m_microPhysRes, 0, sizeof(microPhysicsParams));

	calculateEnvMicroPhysicsGPU<< <GRIDSIZESKYY, GRIDSIZESKYX >> >(m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		dt, speed, m_envGrid.potTemp, m_densityAir, m_envGrid.pressure, m_GHeight, m_groundGrid.P,
		m_condens, m_depos, m_freeze, Game.DataClass().microPhysCheckActive, Game.DataClass().microPhysMinPos, Game.DataClass().microPhysMaxPos, *m_microPhysRes);
	cudaDeviceSynchronize();

	//Add data to the data class
	if (Game.DataClass().microPhysCheckActive)
	{
		Game.DataClass().setMicroPhysicsData(m_microPhysRes);
	}
	cudaDeviceSynchronize();


	//Compute heat and add it to the potTemp
	addHeatGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.Qv, m_envGrid.potTemp, m_condens, m_depos, m_freeze);
	cudaDeviceSynchronize();

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::diffuseGPU(float* diffuseArray, int type, const float dt)
{
	dim3 grid(GRIDSIZEX, GRIDSIZEY);
	dim3 block(BLOCKSIZEX, BLOCKSIZEY);

	float* defaultArray = nullptr;
	boundsEnv boundsConditions{ NEUMANN, DIRICHLET, DIRICHLET };

	switch (type)
	{
	case 0: //Temp
		defaultArray = m_isentropicTemp;
		// Sides boundaries are not actually dirichlet, but we customize inside the function for temp
		boundsConditions.sides = CUSTOM;
		break;
	case 1: //Vapor
		defaultArray = m_isentropicVapor;
		boundsConditions.ground = NEUMANN;
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

	for (int L = 0; L < LOOPS; L++)
	{
		diffuseRedBlack << <grid, block >> > (m_neighbourData, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_array, m_outputArray, k, type, boundsConditions, true);
		cudaDeviceSynchronize();
		diffuseRedBlack << <grid, block >> > (m_neighbourData, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_array, m_outputArray, k, type, boundsConditions, false);
		cudaDeviceSynchronize();

		//Switch output and input around
		diffuseRedBlack << <grid, block >> > (m_neighbourData, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_outputArray, m_array, k, type, boundsConditions, true);
		cudaDeviceSynchronize();
		diffuseRedBlack << <grid, block >> > (m_neighbourData, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_outputArray, m_array, k, type, boundsConditions, false);
		cudaDeviceSynchronize();
	}
	cudaMemcpy(diffuseArray, m_array, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);

	err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

}

void environmentGPU::advectGroundWater(const float dt, const float speed)
{
	// For the ground we use also 16 x 16 blocks when necessary, but now X and Z instead of Y
	dim3 grid(GRIDSIZEX, GRIDSIZEY);
	dim3 block(BLOCKSIZEX, BLOCKSIZEY);

	//Need red-black due to accessing previous index
	advectGroundWaterGPU<<<grid, block>>> (m_groundGrid.Qrs, m_groundGrid.Qgr, dt, speed);
	cudaDeviceSynchronize();
}

void environmentGPU::setTempsAtGround(const float dt, const float speed)
{
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYZ;

	setTempsAtGroundGPU<<<blocks, threads>>>(m_envGrid.potTemp, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, dt * speed);
	cudaDeviceSynchronize();
}

void environmentGPU::advectPPMWGPU(float* advectArray, const float* defaultVal, const float dt)
{
	dim3 grid(GRIDSIZEX, GRIDSIZEY);
	dim3 block(BLOCKSIZEX, BLOCKSIZEY);

	//Create events to track time taking of parts
	//cudaEvent_t start;
	//cudaEvent_t stop;
	//cudaEventCreate(&start);
	//cudaEventCreate(&stop);
	//cudaEventRecord(start);


	//Reset data
	cudaMemset(m_outputArray, 0, GRIDSIZESKY * sizeof(float));
	cudaMemset(m_stor0, 0, GRIDSIZESKY * sizeof(float));
	setToValue << <GRIDSIZESKYY, 1 >> > (m_dummyArraySky2, 1.0f, 1);

	//Recollect all threads before starting kernels
	cudaDeviceSynchronize();

	boundsEnv boundsX{ CUSTOM, CUSTOM, DIRICHLET };
	boundsEnv boundsY{ NEUMANN, DIRICHLET, DIRICHLET };
	boundsEnv boundsZ{ CUSTOM, CUSTOM, DIRICHLET };

	// Substeps based on highest courant number
	// This is to make sure that the advecting value can not pass over 1 cell, for example:
	// If velocity is too high, the value actually never passes this cell in one time frame but passes over.
	// To fix this we use sub-steps.
	cudaMemset(m_singleStor0, 0, sizeof(float));
	// Get max velocity and base C of this.
	getMaxDivergence << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_singleStor0, m_envGrid.velfieldX);
	getMaxDivergence << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_singleStor0, m_envGrid.velfieldY);
	getMaxDivergence << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_singleStor0, m_envGrid.velfieldZ);
	float maxVel = 0.0f;
	cudaMemcpy(&maxVel, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);

	const float C = maxVel * dt / VOXELSIZE;
	const float MAXSUBSTEPS = 100;

	const int subSteps = int(fminf(ceilf(fabsf(C)), MAXSUBSTEPS));
	const float dtSub = dt / subSteps;
	if (C > 100)
	{
		printf("WARNING: Courant number is high: %f, increase size of voxels or decrease timesteps, %f, %i\n", C, dtSub, subSteps);
	}

	// We advect the default value and density. 
	// After each time we advect, we need to divide by the density, this is to make sure any errors that accumulate are immediately resolved.
	// Using half x, half y and full z, we use second order accuracy, making sure we treat x, y and z equally. 
	for (int i = 0; i < subSteps; i++)
	{
		// Using strang method
		// Also used in flash https://flash.rochester.edu/site/flashcode/user_support/flash2_users_guide/docs/FLASH2.5/flash2_ug.pdf
		// in 6.1.3, strang is done using X, Y, Z, then another timestep for Z, Y, X. Combining that in 1 timestep, we can do:
		// 0.5X, 0.5Y, Z, 0.5Y, 0.5X
		

		//Its kernel time, advecting 2 times half X, 2 times half Y and 1 time Z
		
		//------------------------------ 0.5 X -----------------------------------

		//First advect density and array on half X
		advectPPMX << <grid, block >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldX, m_neighbourData, boundsX, dtSub * 0.5f);
		advectPPMX << <grid, block >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, boundsX, dtSub * 0.5f);
		cudaDeviceSynchronize();

		//Divide to get them both up to speed
		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_stor0, GRIDSIZESKYX);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYX);
		cudaDeviceSynchronize();

		//------------------------------ 0.5 Y -----------------------------------

		//Switch blocks and threads around (x and y are swapped)
		//Also input and output are swapped because of previous result
		advectPPMY << <grid, block >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldY, m_neighbourData, boundsY, dtSub * 0.5f);
		advectPPMY << <grid, block >> > (m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, boundsY, dtSub * 0.5f);
		cudaDeviceSynchronize();

		//Divide to get them both up to speed
		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (advectArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		cudaDeviceSynchronize();

		//------------------------------ 1.0 Z -----------------------------------

		advectPPMZ << <grid, block >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldZ, m_neighbourData, boundsZ, dtSub);
		advectPPMZ << <grid, block >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldZ, m_neighbourData, boundsZ, dtSub);
		cudaDeviceSynchronize();

		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		cudaDeviceSynchronize();

		//------------------------------ 0.5 Y -----------------------------------

		advectPPMY << <grid, block >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldY, m_neighbourData, boundsY, dtSub * 0.5f);
		advectPPMY << <grid, block >> > (m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, boundsY, dtSub * 0.5f);
		cudaDeviceSynchronize();

		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (advectArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		cudaDeviceSynchronize();

		//------------------------------ 0.5 X -----------------------------------

		advectPPMX << <grid, block >> > (m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldX, m_neighbourData, boundsX, dtSub * 0.5f);
		advectPPMX << <grid, block >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, boundsX, dtSub * 0.5f);
		cudaDeviceSynchronize();

		divideValues << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_outputArray, m_stor0, GRIDSIZESKYZ);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYZ);
		cudaDeviceSynchronize();

		//Switch over input and output for our next iteration or return result.
		cudaMemcpy(advectArray, m_outputArray, sizeof(GRIDSIZESKY) * sizeof(float), cudaMemcpyDeviceToDevice);
	}

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
	const int threads = GRIDSIZESKYY;
	const int blocks = GRIDSIZESKYX;

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
		cudaDeviceSynchronize();
	}
}

void environmentGPU::pressureProject(const float dt)
{
	dim3 grid(GRIDSIZEX, GRIDSIZEY);
	dim3 block(BLOCKSIZEX, BLOCKSIZEY);

	boundsEnv boundsDensity{ NEUMANN, DIRICHLET, DIRICHLET };
	boundsEnv boundsPresX{ CUSTOM, CUSTOM, DIRICHLET };
	boundsEnv boundsPresY{ NEUMANN, DIRICHLET, DIRICHLET };
	boundsEnv boundsPresZ{ CUSTOM, CUSTOM, DIRICHLET };

	//Should not be needed if handled correctly in advection and other velocity updates?
	resetVelPressProj << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ);
	cudaDeviceSynchronize();

	//Initialize A and precon every frame due to changed based on density
	initAMatrix << <grid, block >> > (m_A, m_neighbourData, m_densityAir, boundsDensity);
	cudaDeviceSynchronize();
	initPrecon << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_precon, m_A);
	cudaDeviceSynchronize();

	//Actual pressure project
	calculatePressureProject(m_outputArray, dt);

	//Set new density
	updatePressure << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_envGrid.pressure, m_outputArray);
	initDensity << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
	cudaDeviceSynchronize();

	//Apply calculate pressure to velocity field
	applyPresProjGPU << <grid, block >> > (m_outputArray, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_densityAir, m_envGrid.pressure,
		boundsPresX, boundsPresY, boundsPresZ, boundsDensity, dt);
	cudaDeviceSynchronize();

	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::calculatePressureProject(float* outputPressure, const float dt)
{
	dim3 grid(GRIDSIZEX, GRIDSIZEY);
	dim3 block(BLOCKSIZEX, BLOCKSIZEY);

	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	boundsEnv boundsVel{ CUSTOM, CUSTOM, DIRICHLET };
	boundsEnv boundsDensity{ NEUMANN, DIRICHLET, DIRICHLET };

	const float tolValue = 1e-5f;
	const int MAXITERATION = 200;
	float maxr = 0.0f;

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
	calculateDivergenceGPU << <grid, block >> > (m_stor0, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_densityAir, m_oldDensityAir, boundsDensity, boundsVel, dt);
	cudaDeviceSynchronize();
	Game.Editor().setDebugValueNum(m_stor0, 2);

	//Check max divergence
	getMaxDivergence << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_singleStor0, m_stor0);
	cudaDeviceSynchronize();
	cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
	if (maxr == 0.0f) return;

	applyPreconditionerGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_stor1, m_precon, m_stor0, m_A);
	cudaDeviceSynchronize();


	cudaMemcpy(m_stor2, m_stor1, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
	dotProductGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_sigma0, m_stor1, m_stor0);
	cudaDeviceSynchronize();


	for (int i = 0; i < MAXITERATION; i++)
	{
		// Same bounds as density since A = density
		applyAGPU<<<grid, block>>>(m_stor1, m_stor2, m_neighbourData, m_A, boundsDensity);
		cudaDeviceSynchronize();


		dotProductGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_sigma1, m_stor1, m_stor2);
		cudaDeviceSynchronize();

		//Update the pressure (output) and divergence
		updatePandDiv << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_sigma0, m_sigma1, outputPressure, m_stor0, m_stor2, m_stor1);
		cudaDeviceSynchronize();


		//Check max divergence
		cudaMemset(m_singleStor0, 0, sizeof(float));
		getMaxDivergence << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_singleStor0, m_stor0);
		cudaDeviceSynchronize();
		cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
		if (maxr <= tolValue)
		{
			//printf("Iterations pressure projection: %i\n", i);
			return;
		}

		applyPreconditionerGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_stor1, m_precon, m_stor0, m_A);

		cudaMemset(m_sigma1, 0, sizeof(float));
		cudaDeviceSynchronize();

		//Dotproduct
		dotProductGPU << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_sigma1, m_stor1, m_stor0);
		cudaDeviceSynchronize();

		//Set values and set search vector
		endIteration << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_sigma0, m_sigma1, m_stor2, m_stor1);
		cudaDeviceSynchronize();
	}
	//printf("Max iterations reached!\n");
}

void environmentGPU::editorDataGPU()
{
	//Set/Get editor data
	if (m_groundChanged || Game.Editor().changedGround())
	{
		const int threads = GRIDSIZESKYX;
		const int blocks = GRIDSIZESKYY;

		//Reset values that are in ground
		resetValueInGround << <blocks, threads >> > (m_envGrid.potTemp, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.Qv, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.Qw, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.Qc, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.Qr, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.Qs, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.Qi, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.velfieldX, m_GHeight);
		resetValueInGround << <blocks, threads >> > (m_envGrid.velfieldY, m_GHeight);
		cudaDeviceSynchronize();

		computeNeighbourGPU << <blocks, threads >> > (m_neighbourData);
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
	cudaDeviceSynchronize();
}

void environmentGPU::updateGroundTemps(const float dt, const float speed, const float irridiance)
{
	calculateGroundTempGPU << <GRIDSIZESKYZ, GRIDSIZESKYX >> > (m_groundGrid.T, dt * speed, irridiance, m_dummyArrayGround);
	cudaDeviceSynchronize();
}

void environmentGPU::calculateBuoyancy(const float dt)
{
	dim3 grid(GRIDSIZEX, GRIDSIZEY);
	dim3 block(BLOCKSIZEX, BLOCKSIZEY);

	const float mDistance = 1.0f;// TODO: probably always 1.0f, Distance that buoyancy will be taken into account.
	
	// Neumann, since we will just use the current temp but divide by 2, meaning we don't actually use outside or ground
	boundsEnv buoyancyBounds{ NEUMANN, NEUMANN, NEUMANN };

	buoyancyGPU << <grid, block >> > (m_envGrid.velfieldY, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, m_envGrid.pressure, m_groundGrid.P, m_stor0, mDistance, buoyancyBounds, dt);
	cudaDeviceSynchronize();
	//Set debug
	Game.Editor().setDebugValueNum(m_stor0, 1);

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

float* environmentGPU::getParamArray(parameter type, const bool windX)
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
		return windX ? m_envGrid.velfieldX : m_envGrid.velfieldY;
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

void environmentGPU::prepareBrushGPU(parameter paramType, const float brushSize, const float2 mousePos, const float2 extras, const float brushSmoothnes, const float dt, const float brushIntensity, const float applyValue, const float2 valueDir, const bool groundErase)
{
	cudaMemcpy(m_dummyGHeight, m_GHeight, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToDevice);
	bool changedGround = false;
	cudaMemset(m_storBool, 0, sizeof(bool));

	int blocks = int(std::ceil(brushSize)) - int(std::floor(-brushSize));
	int threads = blocks;

	//Get correct array to possibly change
	float* array = nullptr;
	float* array2 = nullptr;
	if (paramType == WIND)
	{
		array = getParamArray(paramType);
		array2 = getParamArray(paramType, false);
	}
	else if (paramType != PGROUND)
	{
		array = getParamArray(paramType);
	}
	array = m_envGrid.pressure;
	//Actually calculate where to put brush and apply brush values
	applyBrushGPU << <blocks, threads >> > (array, array2, m_dummyGHeight, m_storBool, paramType, brushSize, mousePos, extras, brushSmoothnes, brushIntensity, applyValue, valueDir, groundErase, m_neighbourData, dt);
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
		threads = GRIDSIZESKYX;
		blocks = GRIDSIZESKYY;

		//First set all data back that was in the ground before
		if (groundErase)
		{
			compareAndResetValuesOutGround << <blocks, threads >> > (m_GHeight, m_dummyGHeight, m_isentropicTemp, m_isentropicVapor,
				m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, 
				m_envGrid.potTemp, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.pressure, m_defaultPressure);
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
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;
	if (paramType == POTTEMP) setToDefault << <blocks, threads >> > (m_envGrid.potTemp, m_isentropicTemp);
	if (paramType == WIND)
	{
		setToDefault << <blocks, threads >> > (m_envGrid.velfieldX, m_defaultVelX);
		setToValue << <blocks, threads >> > (m_envGrid.velfieldY, 0.0f, GRIDSIZESKYX);
		setToDefault << <blocks, threads >> > (m_envGrid.velfieldZ, m_defaultVelZ);
	}
	if (paramType == PRESSURE)
	{
		setToDefault << <blocks, threads >> > (m_envGrid.pressure, m_defaultPressure);
	}
	if (paramType == QV) setToDefault << <blocks, threads >> > (m_envGrid.Qv, m_isentropicVapor);
	if (paramType == QW) setToValue << <blocks, threads >> > (m_envGrid.Qw, 0.0f, GRIDSIZESKYX);
	if (paramType == QC) setToValue << <blocks, threads >> > (m_envGrid.Qc, 0.0f, GRIDSIZESKYX);
	if (paramType == QR) setToValue << <blocks, threads >> > (m_envGrid.Qr, 0.0f, GRIDSIZESKYX);
	if (paramType == QS) setToValue << <blocks, threads >> > (m_envGrid.Qs, 0.0f, GRIDSIZESKYX);
	if (paramType == QI) setToValue << <blocks, threads >> > (m_envGrid.Qi, 0.0f, GRIDSIZESKYX);
}

envDebugData* environmentGPU::getDebugData()
{
	return new envDebugData();
};