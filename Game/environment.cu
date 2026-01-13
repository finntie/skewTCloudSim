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
#include "kernelSky.cuh"
#include "kernelGround.cuh"
#include "microPhysics.cuh"

#include "math/meteoformulas.h"
#include "meteoformulas.cuh"
#include "math/geometry.hpp"

#include "editor.h"
#include "game.h"

#define BLOCKSIZE GRIDSIZESKYX
#define BLOCKSIZEY GRIDSIZESKYY

environmentGPU::environmentGPU()
{
	// Malloc space for the pointers
	// Current allocated space:
	//const int totalAllocatedSpace = (21 * GRIDSIZESKY) + (10 * GRIDSIZEGROUND) + (5 * GRIDSIZESKYY) + 2;

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

	cudaMalloc((void**)&m_array, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_outputArray, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_defaultVal, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_neighbourData, GRIDSIZESKY * sizeof(Neigh));
	cudaMalloc((void**)&m_density, GRIDSIZESKY * sizeof(float));

	//Default values
	cudaMalloc((void**)&m_pressures, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_defaultVel, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_isentropicTemp, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_isentropicVapor, GRIDSIZESKYY * sizeof(float));
	cudaMalloc((void**)&m_dummyArray, GRIDSIZESKYY * sizeof(float));

	//Heat
	cudaMalloc((void**)&m_condens, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_depos, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_freeze, GRIDSIZESKY * sizeof(float));

	//Single values
	cudaMalloc((void**)&m_singleStor0, sizeof(float));
	cudaMalloc((void**)&m_sigma0, sizeof(float));
	cudaMalloc((void**)&m_sigma1, sizeof(float));
	cudaMalloc((void**)&m_firstValid, sizeof(int));


	//Extra storage
	cudaMalloc((void**)&m_stor0, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_stor1, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_stor2, GRIDSIZESKY * sizeof(float));
	cudaMalloc((void**)&m_A, GRIDSIZESKY * sizeof(char3));
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

	setToValue << <1, GRIDSIZEGROUND >> > (m_groundGrid.Qrs, 0.001f, GRIDSIZEGROUND);
	cudaMemset(m_groundGrid.Qgr, 0, GRIDSIZEGROUND * sizeof(float));
	cudaMemset(m_groundGrid.Qgs, 0, GRIDSIZEGROUND * sizeof(float));
	cudaMemset(m_groundGrid.Qgi, 0, GRIDSIZEGROUND * sizeof(float));
	setToValue << <1, GRIDSIZEGROUND >> > (m_groundGrid.P, 1000.0f, GRIDSIZEGROUND);
	setToValue << <1, GRIDSIZEGROUND >> > (m_groundGrid.T, 315.15f, GRIDSIZEGROUND);
	cudaMemset(m_groundGrid.t, 0, GRIDSIZEGROUND * sizeof(float));
}

environmentGPU::~environmentGPU()
{
	//Free space
	cudaFree(m_precon);
	cudaFree(m_A);
	cudaFree(m_stor2);
	cudaFree(m_stor1);
	cudaFree(m_stor0);

	cudaFree(m_firstValid);
	cudaFree(m_sigma1);
	cudaFree(m_sigma0);
	cudaFree(m_singleStor0);

	cudaFree(m_freeze);
	cudaFree(m_depos);
	cudaFree(m_condens);

	cudaFree(m_dummyArray);
	cudaFree(m_isentropicVapor);
	cudaFree(m_isentropicTemp);
	cudaFree(m_defaultVel);
	cudaFree(m_pressures);

	cudaFree(m_density);
	cudaFree(m_neighbourData);
	cudaFree(m_defaultVal);
	cudaFree(m_outputArray);
	cudaFree(m_array);

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

void environmentGPU::init(float* potTemps, glm::vec2* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures)
{
	cudaError_t err;
	err = cudaMemcpy(m_envGrid.potTemp, potTemps, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	if (err != cudaSuccess) {
		std::cerr << "potTemp copy failed: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
	cudaMemcpy(potTemps, m_envGrid.potTemp, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);

	{
		float velX[GRIDSIZESKY];
		float velY[GRIDSIZESKY];

		for (int i = 0; i < GRIDSIZESKY; i++)
		{
			velX[i] = velField[i].x;
			velY[i] = velField[i].y;
		}
		cudaMemcpy(m_envGrid.velfieldX, velX, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
		cudaMemcpy(m_envGrid.velfieldY, velY, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);
	}
	cudaMemcpy(m_envGrid.Qv, Qv, GRIDSIZESKY * sizeof(float), cudaMemcpyHostToDevice);

	cudaMemcpy(m_groundGrid.T, groundTemp, GRIDSIZEGROUND * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(groundTemp, m_groundGrid.T, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);

	cudaMemcpy(m_groundGrid.P, groundPres, GRIDSIZEGROUND * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(m_pressures, pressures, GRIDSIZESKYY * sizeof(float), cudaMemcpyHostToDevice);

	//Set values using offset, with the 2D, you can add an offset
	cudaMemcpy2D(m_isentropicTemp, sizeof(float), m_envGrid.potTemp, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	cudaMemcpy2D(m_isentropicVapor, sizeof(float), m_envGrid.Qv, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	cudaMemcpy2D(m_defaultVel, sizeof(float), m_envGrid.velfieldX, GRIDSIZESKYX * sizeof(float), sizeof(float), GRIDSIZESKYY, cudaMemcpyHostToDevice);
	setToValue << <1, GRIDSIZESKYY >> > (m_dummyArray, 1.0f, GRIDSIZESKYX);
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
	PNoise1D(100, noise, GRIDSIZEGROUND, GRIDSIZEGROUND);

	cudaMemcpy(noiseGPU, noise, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDefault);
	const float maxHeight = 0;
	initGroundHeightGPU << <1, GRIDSIZEGROUND >> > (m_GHeight, noiseGPU, maxHeight);
	cudaDeviceSynchronize();
	cudaFree(noiseGPU);

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Init Editor
	Game.Editor().setIsentropics(m_isentropicTemp, m_isentropicVapor, m_pressures);
	Game.Editor().setTime(m_time);
	Game.Editor().setLongitude(m_longitude);
	Game.Editor().setDay(m_day);

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
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	computeNeighbourGPU << <blocks, threads >> > (m_neighbourData);
	cudaDeviceSynchronize();
	computeIsenTempGroundGPU << <1, threads >> > (m_groundGrid.T, m_isentropicTemp, m_groundGrid.P, m_pressures, m_GHeight);
	getFirstValidIndex << <1, 1 >> > (m_firstValid, m_GHeight);
	cudaDeviceSynchronize();


	//Init other classes
	initKernelSky(m_GHeight, m_defaultVel);
	initGammas();
	initGammasMicroPhysics();
	//Set editor data
	Game.Editor().GPUSetEnv(&m_envGrid, &m_groundGrid);

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

	resetValues();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
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
		advectGroundWaterGPU(dt, speed);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
		// 2.
		groundCoverageFactorGPU();
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		// 3.
		updateGroundTempsGPU(dt, speed, irridiance);
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
		
		// --	Loop	--
		// 4. Regain Sky Temps and density
		// 5. Update microphysics
		// 6. Compute heat transfer (form. 67)


		// 1. 
		diffuseGPU(m_envGrid.potTemp, 0, dt * speed);
		advectAddDensity(m_envGrid.potTemp, m_isentropicTemp, dt, true);
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		// 2.
		calculateBuoyancy(dt);
		diffuseGPU(m_envGrid.velfieldX, 2, dt * speed);
		diffuseGPU(m_envGrid.velfieldY, 3, dt * speed);
		advectAddDensity(m_envGrid.velfieldX, m_defaultVel, dt, true);
		advectAddDensity(m_envGrid.velfieldY, nullptr, dt, true); //Also density due to field changed
		pressureProject();
		err = cudaGetLastError();
		if (err != cudaSuccess) {
			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}
		
		// 3.	
		diffuseGPU(m_envGrid.Qv, 1, dt * speed);//Vapor
		advectAddDensity(m_envGrid.Qv, m_isentropicVapor, dt, true);
		err = cudaGetLastError();
		if (err != cudaSuccess) {

			std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
			__debugbreak();
		}

		diffuseGPU(m_envGrid.Qw, 4, dt* speed);//Water cloud
		advectAddDensity(m_envGrid.Qw, m_dummyArray, dt, false);

		diffuseGPU(m_envGrid.Qc, 4, dt * speed);//Ice cloud
		advectAddDensity(m_envGrid.Qc, m_dummyArray, dt, false);
		
		diffuseGPU(m_envGrid.Qr, 4, dt * speed);//Rain
		advectPrecip(m_envGrid.Qr, 0, dt);
		advectAddDensity(m_envGrid.Qr, m_dummyArray, dt, false);

		diffuseGPU(m_envGrid.Qs, 4, dt* speed);//Snow
		advectPrecip(m_envGrid.Qs, 1, dt);
		advectAddDensity(m_envGrid.Qs, m_dummyArray, dt, false);

		diffuseGPU(m_envGrid.Qi, 4, dt * speed);//Ice
		advectPrecip(m_envGrid.Qi, 2, dt);
		advectAddDensity(m_envGrid.Qi, m_dummyArray, dt, false);
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
	Game.Editor().GPUSetEnv(&m_envGrid, &m_groundGrid);
}

void environmentGPU::microPhysicsGroundGPU(const float dt, const float speed, const float irradiance)
{
	const int threads = GRIDSIZESKYX;

	calculatePrecipHittingGroundMicroPhysicsGPU << <1, threads >> > (m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		m_groundGrid.Qgr, m_groundGrid.Qgs, m_groundGrid.Qgi, dt, speed, m_groundGrid.T, m_envGrid.potTemp,
		m_pressures, m_groundGrid.P, m_envGrid.velfieldX, m_GHeight);

	calculateGroundMicroPhysicsGPU << <1, threads >> > (m_groundGrid.Qrs, m_envGrid.Qv, m_groundGrid.Qgr, m_groundGrid.Qgs, m_groundGrid.Qgi,
		dt, speed, m_groundGrid.T, m_envGrid.potTemp, m_pressures, m_groundGrid.P, m_groundGrid.t, irradiance, m_envGrid.velfieldX, m_dummyArrayGround, m_GHeight);
	cudaDeviceSynchronize();
}

void environmentGPU::microPhysicsSkyGPU(const float dt, const float speed)
{
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	calculateEnvMicroPhysicsGPU << <blocks, threads >> > (m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		dt, speed, m_envGrid.potTemp, m_pressures, m_GHeight, m_groundGrid.P,
		m_condens, m_depos, m_freeze);
	cudaDeviceSynchronize();
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	//Compute heat and add it to the potTemp
	addHeatGPU << <blocks, threads >> > (m_envGrid.Qv, m_envGrid.potTemp, m_condens, m_depos, m_freeze);
	cudaDeviceSynchronize();
}

void environmentGPU::diffuseGPU(float* diffuseArray, int type, const float dt)
{
	//TODO: shared memory somewhere?

	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	float* defaultArray = nullptr;

	switch (type)
	{
	case 0: //Temp
		defaultArray = m_isentropicTemp;
		break;
	case 1: //Vapor
		defaultArray = m_isentropicVapor;
		break;
	case 2: //Vel-X
		defaultArray = m_defaultVel;
		break;
	case 3: //Vel-Y
		defaultArray = m_defaultVel; //Not going to even use this
		break;
	case 4: //Default
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
		diffuseRed << <blocks, threads >> > (m_neighbourData, m_groundGrid.T, m_pressures, m_groundGrid.P, defaultArray, m_array, m_outputArray, k, type);
		cudaDeviceSynchronize();
		diffuseBlack << <blocks, threads >> > (m_neighbourData, m_groundGrid.T, m_pressures, m_groundGrid.P, defaultArray, m_array, m_outputArray, k, type);
		cudaDeviceSynchronize();

		//Switch output and input around
		diffuseRed << <blocks, threads >> > (m_neighbourData, m_groundGrid.T, m_pressures, m_groundGrid.P, defaultArray, m_outputArray, m_array, k, type);
		cudaDeviceSynchronize();
		diffuseBlack << <blocks, threads >> > (m_neighbourData, m_groundGrid.T, m_pressures, m_groundGrid.P, defaultArray, m_outputArray, m_array, k, type);
		cudaDeviceSynchronize();
	}
	cudaMemcpy(diffuseArray, m_array, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);

	err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

}

void environmentGPU::advectGroundWaterGPU(const float dt, const float speed)
{
	const int threads = GRIDSIZESKYX;

	cudaMemcpy(m_dummyArrayGround, m_groundGrid.Qrs, GRIDSIZEGROUND * sizeof(float), cudaMemcpyHostToDevice);
	cudaMemcpy(m_dummyArrayGround2, m_groundGrid.Qgr, GRIDSIZEGROUND * sizeof(float), cudaMemcpyHostToDevice);

	//Need red-black due to accessing previous index
	advectGroundWaterRed << <1, threads >> > (m_dummyArrayGround, m_dummyArrayGround2, m_groundGrid.Qrs, m_groundGrid.Qgr, dt, speed);
	cudaDeviceSynchronize();
	advectGroundWaterBlack << <1, threads >> > (m_dummyArrayGround, m_dummyArrayGround2, m_groundGrid.Qrs, m_groundGrid.Qgr, dt, speed);
	cudaDeviceSynchronize();
}

void environmentGPU::advectAddDensity(float* array, const float* defaultVal, const float dt, const bool density)
{
	//Advect using density to make sure we keep mass the same
	//If density is false, we use the old density
	if (density)
	{
		setToValue << <1, GRIDSIZESKYY >> > (m_dummyArray, 1.0f, GRIDSIZESKYX);
		setToValue << <GRIDSIZESKYY, GRIDSIZESKYX >> > (m_density, 1.0f, GRIDSIZESKYX);
		advectPPMWGPU(m_density, m_dummyArray, dt);
	}

	advectPPMWGPU(array, defaultVal, dt);

	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;
	divideValues << <blocks, threads >> > (array, m_density, GRIDSIZESKYX);
}

void environmentGPU::advectPPMWGPU(float* advectArray, const float* defaultVal, const float dt)
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
	cudaMemset(m_outputArray, 0, GRIDSIZESKY * sizeof(float));


	//Recollect all threads before starting kernels
	cudaDeviceSynchronize();
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {

		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}


	//TODO: add offset
	//for (int i = 0; i < numOvertimes; i++)
	{
		//Its kernel time, advecting 2 times half X and 1 time Y
		advectPPMX << <blocks, threads >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, dt * 0.5f);
		cudaDeviceSynchronize();

		//Switch blocks and threads around (x and y are swapped)
		advectPPMY << <threads, blocks >> > (m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, dt);
		cudaDeviceSynchronize();

		advectPPMX << <blocks, threads >> > (advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, dt * 0.5f);
		cudaDeviceSynchronize();
	}

	err = cudaGetLastError();
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
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	//Advect Red-Black, Gauss-Seidel strategy.
	advectPrecipRed << <blocks, threads >> > (array, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, m_pressures, m_groundGrid.P, fallVelType, dt);
	advectPrecipBlack << <blocks, threads >> > (array, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, m_pressures, m_groundGrid.P, fallVelType, dt);
	cudaDeviceSynchronize();
}

void environmentGPU::pressureProject()
{
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	//Should not be needed if handled correctly in advection and other velocity updates?
	resetVelPressProj << <threads, blocks >> > (m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY);

	//CPU calculation due to precon not able to be parralized
	if (m_groundChanged)
	{
		initAandPrecon << <threads, blocks >> > (m_A, m_precon, m_neighbourData);
	}
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}

	calculatePressureProject(m_outputArray);
	Game.Editor().setDebugValueNum(m_outputArray, 2);

	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}


	applyPresProjGPU << <blocks, threads >> > (m_outputArray, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY);
	cudaDeviceSynchronize();
	err = cudaGetLastError();
	if (err != cudaSuccess) {
		std::cerr << "Cuda error: " << cudaGetErrorString(err) << std::endl;
		__debugbreak();
	}
}

void environmentGPU::calculatePressureProject(float* outputPressure)
{
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	const float tolValue = 1e-6f;
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
	calculateDivergenceGPU << <blocks, threads >> > (m_stor0, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY);
	cudaDeviceSynchronize();


	//Check max divergence
	getMaxDivergence << <blocks, threads >> > (m_singleStor0, m_stor0);
	cudaDeviceSynchronize();
	cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
	if (maxr == 0.0f) return;

	applyPreconditionerGPU << <blocks, threads >> > (m_stor1, m_precon, m_stor0);
	cudaDeviceSynchronize();

	cudaMemcpy(m_stor2, m_stor1, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToDevice);
	dotProductGPU << <blocks, threads >> > (m_sigma0, m_stor1, m_stor0);
	cudaDeviceSynchronize();



	for (int i = 0; i < MAXITERATION; i++)
	{
		applyAGPU<<<blocks, threads>>>(m_stor1, m_stor2, m_neighbourData, m_A);
		cudaDeviceSynchronize();


		dotProductGPU << <blocks, threads >> > (m_sigma1, m_stor1, m_stor2);
		cudaDeviceSynchronize();

		//Update the pressure (output) and divergence
		updatePandDiv << <blocks, threads >> > (m_sigma0, m_sigma1, outputPressure, m_stor0, m_stor2, m_stor1);
		cudaDeviceSynchronize();


		//Check max divergence
		cudaMemset(m_singleStor0, 0, sizeof(float));
		getMaxDivergence << <blocks, threads >> > (m_singleStor0, m_stor0);
		cudaDeviceSynchronize();
		cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
		if (maxr <= tolValue) return;


		applyPreconditionerGPU << <blocks, threads >> > (m_stor1, m_precon, m_stor0);

		cudaMemset(m_sigma1, 0, sizeof(float));
		cudaDeviceSynchronize();

		//Dotproduct
		dotProductGPU << <blocks, threads >> > (m_sigma1, m_stor1, m_stor0);
		cudaDeviceSynchronize();



		//Set values and set search vector
		endIteration << <blocks, threads >> > (m_sigma0, m_sigma1, m_stor2, m_stor1);
		cudaDeviceSynchronize();
	}
}

void environmentGPU::resetValues()
{
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	setToValue << <blocks, threads >> > (m_density, 1.0f, GRIDSIZESKYX);
}

void environmentGPU::editorDataGPU()
{
	//Set/Get editor data
	if (Game.Editor().changedGround())
	{
		const int threads = GRIDSIZESKYX;
		const int blocks = GRIDSIZESKYY;
		computeNeighbourGPU << <blocks, threads >> > (m_neighbourData);
		computeIsenTempGroundGPU << <1, threads >> > (m_groundGrid.T, m_isentropicTemp, m_groundGrid.P, m_pressures, m_GHeight);

		//Change first valid index
		getFirstValidIndex << <1, 1 >> > (m_firstValid, m_GHeight);
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

void environmentGPU::groundCoverageFactorGPU()
{
	const int threads = GRIDSIZESKYX;
	calculateCloudCover << <1, threads >> > (m_dummyArrayGround, m_envGrid.Qc, m_envGrid.Qw);
	cudaDeviceSynchronize();
}

void environmentGPU::updateGroundTempsGPU(const float dt, const float speed, const float irridiance)
{
	const int threads = GRIDSIZESKYX;

	calculateGroundTemp << <1, threads >> > (m_groundGrid.T, dt * speed, irridiance, m_dummyArrayGround);
	cudaDeviceSynchronize();
}

void environmentGPU::calculateBuoyancy(const float dt)
{
	const int threads = GRIDSIZESKYX;
	const int blocks = GRIDSIZESKYY;

	const float mDistance = 4096.0f / VOXELSIZE; //Distance that buoyancy will be taken into account.
	
	buoyancyGPU << <blocks, threads >> > (m_envGrid.velfieldY, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_pressures, m_groundGrid.P, m_stor0, mDistance, dt);
	cudaDeviceSynchronize();
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

