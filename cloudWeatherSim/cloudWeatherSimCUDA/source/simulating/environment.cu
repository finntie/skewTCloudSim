#include "simulating/environment.cuh"

// Simulating
#include "simulating/kernelSky.cuh"
#include "simulating/microPhysics.cuh"

// Rendering 
#include "rendering/cuda_render_gl.hpp"

// Math
#include "math/meteoformulas.cuh"
#include "math/cudaMath.cuh"

// Shared
#include "config.hpp"
#include "utils.cuh"
#include "geometry.hpp"

// Extras
#include <sstream>
#include <iostream>
#undef NDEBUG
#include <cassert>
#include <memory>
#include <unordered_map>

// Hip
#include "msvc_defines.h"
#include <hip/hip_runtime.h>

// Grid and Block size based on size of simulation
// Set default to 16, but increase based on threads available. 
dim3 m_gridDim = { 16, 16, 16 };
dim3 m_blockDim = { 2, 2, 2 };

hipStream_t simStream;

environmentGPU::environmentGPU(const simParams& params) : m_params(params)
{
	// Initialize stream, give it the highest priority
	int leastPriority, greatestPriority;
	hipDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);
	hipStreamCreateWithPriority(&simStream, hipStreamNonBlocking, greatestPriority);
	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

environmentGPU::~environmentGPU()
{
	freeData();
}

__global__ void k() {}
void runTest() {
	k << <1, 1 >> > ();
	printf("%s / %s\n", hipGetErrorString(hipGetLastError()),
		hipGetErrorString(hipDeviceSynchronize()));
}

void environmentGPU::allocateData()
{
	if (allocatedData)
	{
		printf("CLOUDLIB MESSAGE: tried to allocate environment.cu data, but data was already allocated\n");
		return;
	}

	hipDeviceSynchronize();
	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "errorTestKernel: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	hipStreamSynchronize(simStream);
	hipError_t e = hipFree(0);                 // forces context creation
	printf("init: %s\n", hipGetErrorString(e));

	// does the runtime know the kernel?
	cudaFuncAttributes attr;
	cudaError_t k = cudaFuncGetAttributes(&attr, (const void*)testkernel); // needs to be in the .cu file
	printf("kernel lookup: %s\n", cudaGetErrorString(k));

	testkernel << <16, 16 >> > ();
	lockGlobal();

	// Set block and grid dimension based on GPU specs.
	getGridBlockDims(m_params, m_gridDim, m_blockDim);
	hipDeviceProp_t prop;
	hipGetDeviceProperties(&prop, 0);
	unsigned int totalThreadsPerBlock = prop.maxThreadsPerBlock / 4; // Use smaller threads so we have more leeway
	unsigned int totalBlocksPerGrid = prop.maxBlocksPerMultiProcessor * prop.multiProcessorCount;
	unsigned int totalRegsPerThread = prop.regsPerBlock / totalThreadsPerBlock;

	canFillAll = unsigned(m_params.sizeZ) <= m_gridDim.z;
	// Fill info
	const float _invBlockSpreadDepth = 1.0f / (float(m_gridDim.z) / float(m_params.sizeZ));
	setConstantsCudaMath(_invBlockSpreadDepth, simStream);
	setConstantsMicroPhys(_invBlockSpreadDepth, simStream);
	setConstantsKernelSky(0.0f, 0.0f, _invBlockSpreadDepth, simStream);
	simKernelInfo.neighbourData = m_neighbourData;

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Simulation size x: %i, y: %i, z: %i\n", m_params.sizeX, m_params.sizeY, m_params.sizeZ);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Max Blocks: %i, Max Threads / 4: %i, Max Register Per Thread: %i\n", totalBlocksPerGrid, totalThreadsPerBlock, totalRegsPerThread);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Block Dimension x: %i y: %i\n", m_blockDim.x, m_blockDim.y);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("Blocks on x axis: %i, Blocks on y axis: %i, Blocks on z axis: %i\n", m_gridDim.x, m_gridDim.y, m_gridDim.z);

	printf("[%sSim info%s] ", "\033[32m", "\033[0m");
	printf("GPU: %s\n", prop.name);



	// Malloc space for the pointers
	// Current allocated space:
	//const int totalAllocatedSpace = ((26 * m_params.size) + (11 * m_params.sizeGround) + (7 * m_params.sizeY) + 5) * sizeof(float);

	size_t freeMem, totalMem;
	hipMemGetInfo(&freeMem, &totalMem);

	// Environment Values
	hipMalloc((void**)&m_envGrid.Qv, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.Qw, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.Qc, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.Qr, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.Qs, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.Qi, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.potTemp, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.velfieldX, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.velfieldY, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.velfieldZ, m_params.size * sizeof(float));
	hipMalloc((void**)&m_envGrid.pressure, m_params.size * sizeof(float));

	// Ground values
	hipMalloc((void**)&m_groundGrid.Qrs, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_groundGrid.Qgr, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_groundGrid.Qgs, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_groundGrid.Qgi, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_groundGrid.P, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_groundGrid.t, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_groundGrid.T, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_dummyArrayGround, m_params.sizeGround * sizeof(float));
	hipMalloc((void**)&m_dummyArrayGround2, m_params.sizeGround * sizeof(float));

	hipMalloc((void**)&m_GHeight, m_params.sizeGround * sizeof(int));
	hipMalloc((void**)&m_dummyGHeight, m_params.sizeGround * sizeof(int));

	hipMalloc((void**)&m_array, m_params.size * sizeof(float));
	hipMalloc((void**)&m_outputArray, m_params.size * sizeof(float));
	hipMalloc((void**)&m_storPres, m_params.size * sizeof(float));
	hipMalloc((void**)&m_neighbourData, m_params.size * sizeof(Neigh));
	hipMalloc((void**)&m_density, m_params.size * sizeof(float));
	hipMalloc((void**)&m_densityAir, m_params.size * sizeof(float));
	hipMalloc((void**)&m_oldDensityAir, m_params.size * sizeof(float));

	hipMalloc((void**)&m_microPhysRes, sizeof(microPhysicsParams));

	//Default values
	hipMalloc((void**)&m_defaultPressure, m_params.sizeY * sizeof(float));
	hipMalloc((void**)&m_defaultVelX, m_params.sizeY * sizeof(float));
	hipMalloc((void**)&m_defaultVelZ, m_params.sizeY * sizeof(float));
	hipMalloc((void**)&m_isentropicTemp, m_params.sizeY * sizeof(float));
	hipMalloc((void**)&m_isentropicVapor, m_params.sizeY * sizeof(float));
	hipMalloc((void**)&m_dummyArray, m_params.sizeY * sizeof(float));
	hipMalloc((void**)&m_dummyArraySky2, m_params.sizeY * sizeof(float));

	//Heat
	hipMalloc((void**)&m_condens, m_params.size * sizeof(float));
	hipMalloc((void**)&m_depos, m_params.size * sizeof(float));
	hipMalloc((void**)&m_freeze, m_params.size * sizeof(float));

	//Single values
	hipMalloc((void**)&m_singleStor0, sizeof(float));
	hipMalloc((void**)&m_sigma0, sizeof(float));
	hipMalloc((void**)&m_sigma1, sizeof(float));
	hipMalloc((void**)&m_firstValid, sizeof(int));
	hipMalloc((void**)&m_storBool, sizeof(bool));

	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	//Extra storage
	hipMalloc((void**)&m_stor0, m_params.size * sizeof(float));
	hipMalloc((void**)&m_stor1, m_params.size * sizeof(float));
	hipMalloc((void**)&m_stor2, m_params.size * sizeof(float));
	hipMalloc((void**)&m_A, m_params.size * sizeof(float4));
	hipMalloc((void**)&m_precon, m_params.size * sizeof(float));

	hipMemsetAsync(m_envGrid.Qv, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.Qw, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.Qc, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.Qr, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.Qs, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.Qi, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.potTemp, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.velfieldX, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.velfieldY, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_envGrid.velfieldZ, 0, m_params.size * sizeof(float), simStream);

	int3 size = make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ);

	setToValue << <m_params.sizeZ, m_params.sizeX, 0, simStream >> > (size, m_groundGrid.Qrs, 0.001f, 1, 0);
	hipMemsetAsync(m_groundGrid.Qgr, 0, m_params.sizeGround * sizeof(float), simStream);
	hipMemsetAsync(m_groundGrid.Qgs, 0, m_params.sizeGround * sizeof(float), simStream);
	hipMemsetAsync(m_groundGrid.Qgi, 0, m_params.sizeGround * sizeof(float), simStream);
	setToValue << <m_params.sizeZ, m_params.sizeX, 0, simStream >> > (size, m_groundGrid.P, 1000.0f, 1, 0);
	setToValue << <m_params.sizeZ, m_params.sizeX, 0, simStream >> > (size, m_groundGrid.T, 315.15f, 1, 0);
	hipMemsetAsync(m_groundGrid.t, 0, m_params.sizeGround * sizeof(float), simStream);

	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	// Init graphs
	initDiffuseGPUGraph(m_envGrid.potTemp, 0);
	initDiffuseGPUGraph(m_envGrid.Qv, 1);
	initDiffuseGPUGraph(m_envGrid.velfieldX, 2);
	initDiffuseGPUGraph(m_envGrid.velfieldY, 3);
	initDiffuseGPUGraph(m_envGrid.velfieldZ, 4);
	initDiffuseGPUGraph(m_envGrid.Qw, 5);
	initDiffuseGPUGraph(m_envGrid.Qc, 5);
	initDiffuseGPUGraph(m_envGrid.Qr, 5);
	initDiffuseGPUGraph(m_envGrid.Qs, 5);
	initDiffuseGPUGraph(m_envGrid.Qi, 5);

	initAdvectPPMWGPUGraph(m_envGrid.potTemp, m_isentropicTemp, BOUNDSTEMP);
	initAdvectPPMWGPUGraph(m_envGrid.velfieldX, m_defaultVelX, BOUNDSVELXZ);
	initAdvectPPMWGPUGraph(m_envGrid.velfieldY, m_dummyArray, BOUNDSVELY);
	initAdvectPPMWGPUGraph(m_envGrid.velfieldZ, m_defaultVelZ, BOUNDSVELXZ);
	initAdvectPPMWGPUGraph(m_envGrid.Qv, m_isentropicVapor, BOUNDSVAPOR);
	initAdvectPPMWGPUGraph(m_envGrid.Qw, m_dummyArray, BOUNDSMIXINGRATIOS);
	initAdvectPPMWGPUGraph(m_envGrid.Qc, m_dummyArray, BOUNDSMIXINGRATIOS);
	initAdvectPPMWGPUGraph(m_envGrid.Qr, m_dummyArray, BOUNDSMIXINGRATIOS);
	initAdvectPPMWGPUGraph(m_envGrid.Qs, m_dummyArray, BOUNDSMIXINGRATIOS);
	initAdvectPPMWGPUGraph(m_envGrid.Qi, m_dummyArray, BOUNDSMIXINGRATIOS);

	initPressureProjectGraph();

	// Check how much memory we used
	size_t freeMem2;
	hipMemGetInfo(&freeMem2, &totalMem);
	size_t memUsed = (freeMem / 1024 / 1024) - (freeMem2 / 1024 / 1024);

	printf("GPU Memory used: %zu\n", memUsed);

	// Last error check to make sure initialization went correct
	hipStreamSynchronize(simStream);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	allocatedData = true;

	unlockGlobal();
}

void environmentGPU::freeData()
{
	if (!allocatedData) return;

	// Make sure the stream is up-to-date
	hipStreamSynchronize(simStream); // TODO: what if kernel is in the middle of its run and we free data? Check if issue, maybe use events.

	lockGlobal();

	//Free space
	hipFree(m_precon);
	hipFree(m_A);
	hipFree(m_stor2);
	hipFree(m_stor1);
	hipFree(m_stor0);

	hipFree(m_storBool);
	hipFree(m_firstValid);
	hipFree(m_sigma1);
	hipFree(m_sigma0);
	hipFree(m_singleStor0);

	hipFree(m_freeze);
	hipFree(m_depos);
	hipFree(m_condens);

	hipFree(m_dummyArraySky2);
	hipFree(m_dummyArray);
	hipFree(m_isentropicVapor);
	hipFree(m_isentropicTemp);
	hipFree(m_defaultVelZ);
	hipFree(m_defaultVelX);
	hipFree(m_defaultPressure);

	hipFree(m_oldDensityAir);
	hipFree(m_densityAir);
	hipFree(m_microPhysRes);
	hipFree(m_density);
	hipFree(m_neighbourData);
	hipFree(m_storPres);
	hipFree(m_outputArray);
	hipFree(m_array);

	hipFree(m_dummyGHeight);
	hipFree(m_GHeight);

	hipFree(m_dummyArrayGround2);
	hipFree(m_dummyArrayGround);
	hipFree(m_groundGrid.T);
	hipFree(m_groundGrid.t);
	hipFree(m_groundGrid.P);
	hipFree(m_groundGrid.Qgi);
	hipFree(m_groundGrid.Qgs);
	hipFree(m_groundGrid.Qgr);
	hipFree(m_groundGrid.Qrs);

	hipFree(m_envGrid.pressure);
	hipFree(m_envGrid.velfieldZ);
	hipFree(m_envGrid.velfieldY);
	hipFree(m_envGrid.velfieldX);
	hipFree(m_envGrid.potTemp);
	hipFree(m_envGrid.Qi);
	hipFree(m_envGrid.Qs);
	hipFree(m_envGrid.Qr);
	hipFree(m_envGrid.Qc);
	hipFree(m_envGrid.Qw);
	hipFree(m_envGrid.Qv);

	// Destroy and clear graphs
	for (auto graph : m_diffuseExecutionGraphs)
	{
		hipGraphExecDestroy(reinterpret_cast<hipGraphExec_t>(graph.second));
	}
	for (auto graph : m_advectExecutionGraphs)
	{
		hipGraphExecDestroy(reinterpret_cast<hipGraphExec_t>(graph.second));
	}
	hipGraphExecDestroy(reinterpret_cast<hipGraphExec_t>(m_pressureprojectExecutionGraph));
	m_diffuseExecutionGraphs.clear();
	m_advectExecutionGraphs.clear();
	m_pressureprojectExecutionGraph = nullptr;


	// Check if everything went right
	hipStreamSynchronize(simStream);
	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	allocatedData = false;

	unlockGlobal();
}

void environmentGPU::init(float* potTemps, glm::vec3* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures, float* smallPressure)
{
	// Allocate data first if we have not done it yet.
	if (!allocatedData) allocateData();

	//Init sky
	hipMemcpyAsync(m_envGrid.potTemp, potTemps, m_params.size * sizeof(float), hipMemcpyHostToDevice, simStream);
	
	//First check
	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	int3 size = make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ);

	{
		float* velX = new float[m_params.size];
		float* velY = new float[m_params.size];
		float* velZ = new float[m_params.size];

		for (int i = 0; i < m_params.size; i++)
		{
			velX[i] = velField[i].x;
			velY[i] = velField[i].y;
			velZ[i] = velField[i].z;
		}
		hipMemcpyAsync(m_envGrid.velfieldX, velX, m_params.size * sizeof(float), hipMemcpyHostToDevice, simStream);
		hipMemcpyAsync(m_envGrid.velfieldY, velY, m_params.size * sizeof(float), hipMemcpyHostToDevice, simStream);
		hipMemcpyAsync(m_envGrid.velfieldZ, velZ, m_params.size * sizeof(float), hipMemcpyHostToDevice, simStream);

		delete[] velX;
		delete[] velY;
		delete[] velZ;
	}

	hipMemcpyAsync(m_envGrid.Qv, Qv, m_params.size * sizeof(float), hipMemcpyHostToDevice, simStream);

	hipMemcpyAsync(m_groundGrid.T, groundTemp, m_params.sizeGround * sizeof(float), hipMemcpyHostToDevice, simStream);
	hipMemcpyAsync(groundTemp, m_groundGrid.T, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);

	hipMemcpyAsync(m_groundGrid.P, groundPres, m_params.sizeGround * sizeof(float), hipMemcpyHostToDevice, simStream);
	hipMemcpyAsync(m_defaultPressure, smallPressure, m_params.sizeY * sizeof(float), hipMemcpyHostToDevice, simStream);
	hipMemcpyAsync(m_envGrid.pressure, pressures, m_params.size * sizeof(float), hipMemcpyHostToDevice, simStream);

	//Initialize density
	initDensity<<<m_params.sizeY, m_params.sizeX, 0, simStream >>>(m_params, m_GHeight, m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
	hipMemcpyAsync(m_oldDensityAir, m_densityAir, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream);
	
	//Set values using offset, with the 2D, you can add an offset
	// This also works for 3D, since we first store the X then Y then Z, so we can just ignore the Z.
	hipMemcpy2DAsync(m_isentropicTemp, sizeof(float), m_envGrid.potTemp, m_params.sizeX * sizeof(float), sizeof(float), m_params.sizeY, hipMemcpyHostToDevice, simStream);
	hipMemcpy2DAsync(m_isentropicVapor, sizeof(float), m_envGrid.Qv, m_params.sizeX * sizeof(float), sizeof(float), m_params.sizeY, hipMemcpyHostToDevice, simStream);
	hipMemcpy2DAsync(m_defaultVelX, sizeof(float), m_envGrid.velfieldX, m_params.sizeX * sizeof(float), sizeof(float), m_params.sizeY, hipMemcpyHostToDevice, simStream);
	hipMemcpy2DAsync(m_defaultVelZ, sizeof(float), m_envGrid.velfieldY, m_params.sizeX * sizeof(float), sizeof(float), m_params.sizeY, hipMemcpyHostToDevice, simStream);
	hipMemcpy2DAsync(m_defaultVelZ, sizeof(float), m_envGrid.velfieldZ, m_params.sizeX * sizeof(float), sizeof(float), m_params.sizeY, hipMemcpyHostToDevice, simStream);
	setToValue <<<m_params.sizeY, 1, 0, simStream >>> (size, m_dummyArray, 1.0f, 1, 0);

	// Randomize the environment a bit
	randomArray <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.potTemp, -0.1f, 0.1f, m_params.sizeZ, 100);
	randomArray <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.Qv, 0.0f, 0.0001f, m_params.sizeZ, 101);

	randomArray <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.velfieldX, 0.5f, 0.5f, m_params.sizeZ, 102);
	randomArray <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.velfieldY, -0.2f, 0.2f, m_params.sizeZ, 103);
	randomArray <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.velfieldZ, 0.5f, 0.5f, m_params.sizeZ, 104);

	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	// Reset pressure
	hipMemsetAsync(m_storPres, 0, m_params.size * sizeof(float), simStream);

	//Init ground
	float* noise = nullptr;
	float* noiseGPU = nullptr;
	hipHostMalloc(&noise, m_params.sizeGround * sizeof(float), hipHostMallocDefault);
	hipMalloc(&noiseGPU, m_params.sizeGround * sizeof(float));
	//Generate noise
	PNoise2D(100, noise, m_params.sizeX, m_params.sizeZ, 12);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
	hipMemcpyAsync(noiseGPU, noise, m_params.sizeGround * sizeof(float), hipMemcpyDefault, simStream);
	const float maxHeight = 0.2f;
	initGroundHeightGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, noiseGPU, maxHeight);
	hipStreamSynchronize(simStream);

	hipFree(noiseGPU);
	hipHostFree(noise);

	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	//Reset values that are in ground
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.potTemp, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qv, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qw, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qc, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qr, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qs, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qi, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.velfieldX, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.velfieldY, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.velfieldZ, m_GHeight);
	hipStreamSynchronize(simStream);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	computeNeighbourGPU <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_neighbourData);
	computeIsenTempGroundGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_groundGrid.T, m_isentropicTemp, m_groundGrid.P, m_envGrid.pressure, m_GHeight);
	hipStreamSynchronize(simStream);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	//Init other classes
	initKernelSky(m_params, m_defaultVelX, m_defaultVelZ, simStream);
	initGammasMicroPhysics(simStream);
	
	hipStreamSynchronize(simStream);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

void environmentGPU::resize()
{
	freeData();
	allocateData();
}

void environmentGPU::updateGPU(const float dt, const float speed)
{
	//Main function that goes through the loop
	hipError_t err = hipGetLastError();

	m_updatingSimulation = true;


	// Set values info
	setConstantsKernelSky(dt, speed, -1.0f, simStream);

	setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ), m_density, 1.0f, m_params.sizeZ, 0);

	// Update ground if changed
	if (m_updateGround) updateOutOfSyncGround();

	hipStreamSynchronize(simStream);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	// 1. Update total incoming solar radiation 
	// 2. Update Ground
	// 3. Update Sky

	// 1.
	const float irridiance = irridianceGPU();
	hipStreamSynchronize(simStream);
	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}



	// 2. ---------GROUND----------
	{
		// 1. Advect microphysics ground
		// 2. Compute Cloud Covering Fraction 
		// 3. Update ground temperature
		// 4. Update microphysic process ground and updateInput ground temp, also precip hitting ground

		// 1.
		advectGroundWater(dt, speed);
		hipStreamSynchronize(simStream);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}
		// 2.
		groundCoverageFactor();
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}

		// 3.
		updateGroundTemps(dt, speed, irridiance);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}

		// 4.
		microPhysicsGroundGPU(dt, speed, irridiance);
		hipStreamSynchronize(simStream);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
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
		advectPPMWGPU(m_envGrid.potTemp, m_isentropicTemp, BOUNDSTEMP, dt * speed);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}


		//2.

		calculateBuoyancy(dt * speed);
		diffuseGPU(m_envGrid.velfieldX, 2, dt * speed);
		diffuseGPU(m_envGrid.velfieldY, 3, dt * speed);
		diffuseGPU(m_envGrid.velfieldZ, 4, dt * speed);
		advectPPMWGPU(m_envGrid.velfieldX, m_defaultVelX, BOUNDSVELXZ, dt * speed);
		hipMemsetAsync(m_dummyArray, 0, m_params.sizeY * sizeof(float), simStream);
		advectPPMWGPU(m_envGrid.velfieldY, m_dummyArray, BOUNDSVELY, dt * speed);
		advectPPMWGPU(m_envGrid.velfieldZ, m_defaultVelZ, BOUNDSVELXZ, dt * speed);

		initDensity <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
		//cudaDeviceSynchronize();
		hipMemcpyAsync(m_oldDensityAir, m_densityAir, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream);
		
		// Debug
		m_debugArray0 = m_densityAir;

		pressureProject(dt * speed);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}
		
		calculateNewPressure <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.pressure, m_densityAir, m_envGrid.potTemp, m_envGrid.Qv, m_groundGrid.P);
		hipStreamSynchronize(simStream);




		// 3.	
		diffuseGPU(m_envGrid.Qv, 1, dt * speed);//Vapor
		advectPPMWGPU(m_envGrid.Qv, m_isentropicVapor, BOUNDSVAPOR, dt * speed);
		err = hipGetLastError();
		if (err != hipSuccess) {

			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}

		diffuseGPU(m_envGrid.Qw, 5, dt * speed);//Water cloud
		advectPPMWGPU(m_envGrid.Qw, m_dummyArray, BOUNDSMIXINGRATIOS, dt* speed);
		
		diffuseGPU(m_envGrid.Qc, 5, dt * speed);//Ice cloud
		advectPPMWGPU(m_envGrid.Qc, m_dummyArray, BOUNDSMIXINGRATIOS, dt * speed);
		
		diffuseGPU(m_envGrid.Qr, 5, dt * speed);//Rain
		advectPrecip(m_envGrid.Qr, 0, dt * speed);
		advectPPMWGPU(m_envGrid.Qr, m_dummyArray, BOUNDSMIXINGRATIOS, dt * speed);
		
		diffuseGPU(m_envGrid.Qs, 5, dt* speed);//Snow
		advectPrecip(m_envGrid.Qs, 1, dt* speed);
		advectPPMWGPU(m_envGrid.Qs, m_dummyArray, BOUNDSMIXINGRATIOS, dt* speed);
		
		diffuseGPU(m_envGrid.Qi, 5, dt * speed);//Ice
		advectPrecip(m_envGrid.Qi, 2, dt * speed);
		advectPPMWGPU(m_envGrid.Qi, m_dummyArray, BOUNDSMIXINGRATIOS, dt * speed);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}

		// 4.	
		microPhysicsSkyGPU(dt, speed);
		err = hipGetLastError();
		if (err != hipSuccess) {
			std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
			assert(false);
		}
	}

	hipStreamSynchronize(simStream);

	//Set editor data
	m_time += speed * dt * float(!m_pauseDiurnal);
	if (m_time > 86400.0f) m_time = 0.0f;

	m_groundChanged = false;
	m_updatingSimulation = false;
}

void environmentGPU::microPhysicsGroundGPU(const float dt, const float speed, const float irradiance)
{
	calculatePrecipHittingGroundMicroPhysicsGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		m_groundGrid.Qgr, m_groundGrid.Qgs, m_groundGrid.Qgi, dt, speed, m_groundGrid.T, m_envGrid.potTemp,
		m_densityAir, m_envGrid.pressure, m_groundGrid.P, m_envGrid.velfieldX, m_GHeight);

	calculateGroundMicroPhysicsGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_groundGrid.Qrs, m_envGrid.Qv, m_groundGrid.Qgr, m_groundGrid.Qgs, m_groundGrid.Qgi,
		dt, speed, m_groundGrid.T, m_envGrid.potTemp, m_densityAir, m_envGrid.pressure, m_groundGrid.P, m_groundGrid.t, irradiance, m_envGrid.velfieldX, m_dummyArrayGround, m_GHeight);
}

void environmentGPU::microPhysicsSkyGPU(const float dt, const float speed)
{
	//Reset
	hipMemsetAsync(m_microPhysRes, 0, sizeof(microPhysicsParams), simStream);
	const int sharedDataSize = m_blockDim.x * m_blockDim.y * sizeof(microPhysicsParams);

	int3 minPos = make_int3(m_microPhysMinPos.x, m_microPhysMinPos.y, m_microPhysMinPos.z);
	int3 maxPos = make_int3(m_microPhysMaxPos.x, m_microPhysMaxPos.y, m_microPhysMaxPos.z);

	calculateEnvMicroPhysicsGPU<<<m_gridDim, m_blockDim, sharedDataSize, simStream >>>(m_params, m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		dt, speed, m_envGrid.potTemp, m_densityAir, m_envGrid.pressure, m_GHeight, m_groundGrid.P,
		m_condens, m_depos, m_freeze, m_microPhysDataActive, minPos, maxPos, *m_microPhysRes);

	//cudaDeviceSynchronize();
	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	//Compute heat and add it to the potTemp
	addHeatGPU <<<m_gridDim, m_blockDim, 0, simStream >>> (m_params, m_envGrid.Qv, m_envGrid.potTemp, m_condens, m_depos, m_freeze);

	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

void environmentGPU::diffuseGPU(float* diffuseArray, int type, const float dt)
{
	// Check if the graph was initialized
    initDiffuseGPUGraph(diffuseArray, type);

	const float k = 0.005f * dt / (m_params.voxelSize * m_params.voxelSize);                                 // Viscosity value
    hipMemcpyAsync(m_singleStor0, &k, sizeof(float), hipMemcpyHostToDevice, simStream);  // Copy over k value
    hipStreamSynchronize(simStream); // Host copy synchronize

	const int LOOPS = 20 / 2;  // Total loops for the Gauss-Seidel method (divided by 2 due to doing 2 times)

	for (int L = 0; L < LOOPS; L++)
	{
		// Now actually launch the graph we created
		hipGraphLaunch(reinterpret_cast<hipGraphExec_t>(m_diffuseExecutionGraphs[diffuseArray]), simStream);
	}


	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {

		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

}

void environmentGPU::initDiffuseGPUGraph(float* diffuseArray, int type) 
{
    if (m_diffuseExecutionGraphs.find(diffuseArray) == m_diffuseExecutionGraphs.end())
	{
        float* defaultArray = nullptr;
        boundsEnv boundsConditions{NEUMANN, DIRICHLET, DIRICHLET};

        switch (type)
        {
            case 0:  // Temp
                defaultArray = m_isentropicTemp;
                // Sides boundaries are not actually dirichlet, but we customize inside the function for temp
                boundsConditions.ground = CUSTOM;
                boundsConditions.sides = CUSTOM;
                boundsConditions.up = CUSTOM;
                break;
            case 1:  // Vapor
                defaultArray = m_isentropicVapor;
                boundsConditions.ground = DIRICHLET;
                boundsConditions.sides = CUSTOM;
                boundsConditions.up = CUSTOM;
                break;
            case 2:  // Vel-X
                defaultArray = m_defaultVelX;
                boundsConditions.sides = CUSTOM;
                boundsConditions.up = CUSTOM;
                break;
            case 3:                      // Vel-Y
                defaultArray = nullptr;  // Not going to even use this
                boundsConditions.sides = NEUMANN;
                boundsConditions.up = DIRICHLET;
                break;
            case 4:                            // Vel-Z
                defaultArray = m_defaultVelZ;  // Not going to even use this
                boundsConditions.sides = CUSTOM;
                boundsConditions.up = CUSTOM;
                break;
            case 5:  // Default
                hipMemsetAsync(m_dummyArray, 0, m_params.sizeY * sizeof(float), simStream);
                defaultArray = m_dummyArray;
                break;
        }

        hipMemcpyAsync(m_array, diffuseArray, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream);
        const int sharedDataSize = (m_blockDim.x + 2) * (m_blockDim.y + 2) * sizeof(float);



		// Use graphs to get rid of the overhaul between kernel launches
        // We only need to initialize the graph ones, afterwards we can reuse the graph.
        // We do need a different graph for each different array, thus we use a map
		hipGraph_t graph;
		hipStreamBeginCapture(simStream, hipStreamCaptureModeGlobal);

		// Diffuse using red and black (chess pattern on which cells will get executed)
		diffuseRedBlack <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_array, m_outputArray, m_singleStor0, type, boundsConditions, true, m_neighbourData);
		diffuseRedBlack <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_array, m_outputArray, m_singleStor0, type, boundsConditions, false, m_neighbourData);

		//Switch output and input around
		diffuseRedBlack <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_outputArray, m_array, m_singleStor0, type, boundsConditions, true, m_neighbourData);
		diffuseRedBlack <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, defaultArray, m_outputArray, m_array, m_singleStor0, type, boundsConditions, false, m_neighbourData);

		hipGraphExec_t newGraph;
		hipStreamEndCapture(simStream, &graph);
		hipGraphInstantiateWithFlags(&newGraph, graph, 0);
		hipGraphDestroy(graph);
		m_diffuseExecutionGraphs[diffuseArray] = reinterpret_cast<hipGraphExec*>(newGraph);

        hipError_t err = hipGetLastError();
        if (err != hipSuccess)
        {
            std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
            assert(false);
        }
	}

}

void environmentGPU::advectGroundWater(const float , const float )
{
	// For the ground we use also use block and gridsize, but now X and Z instead of Y
	dim3 grid(std::min(16, m_params.sizeX), std::min(16, m_params.sizeZ));
	dim3 block((m_params.sizeX + 15) / 16, (m_params.sizeZ + 15) / 16);

	const int sharedDataSize = (block.x + 2) * (block.y + 2) * sizeof(float) * 2;

	advectGroundWaterGPU <<<grid, block, sharedDataSize, simStream >>> (m_params, m_GHeight, m_groundGrid.Qrs, m_groundGrid.Qgr);
}

void environmentGPU::setTempsAtGround(const float dt, const float speed)
{
	setTempsAtGroundGPU<<<m_params.sizeZ, m_params.sizeX, 0, simStream >>>(m_params, m_GHeight, m_envGrid.potTemp, m_groundGrid.T, m_envGrid.pressure, m_groundGrid.P, dt * speed);
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
	hipMemsetAsync(m_outputArray, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_stor0, 0, m_params.size * sizeof(float), simStream);
	int3 size = make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ);
	setToValue <<<m_params.sizeY, 1, 0, simStream >>> (size, m_dummyArraySky2, 1.0f, 1, 0);

	//Recollect all threads before starting kernels
	//cudaDeviceSynchronize();


	// Substeps based on highest courant number
	// This is to make sure that the advecting value can not pass over 1 cell, for example:
	// If velocity is too high, the value actually never passes this cell in one time frame but passes over.
	// To fix this we use sub-steps.
	hipMemsetAsync(m_singleStor0, 0, sizeof(float), simStream);
	const int sharedDataSizeMaxDiv = m_blockDim.x * m_blockDim.y * sizeof(float);

	// Get max velocity and base C of this.
	getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeMaxDiv, simStream >>> (m_params, m_GHeight, m_singleStor0, m_envGrid.velfieldX);
	getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeMaxDiv, simStream >>> (m_params, m_GHeight, m_singleStor0, m_envGrid.velfieldY);
	getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeMaxDiv, simStream >>> (m_params, m_GHeight, m_singleStor0, m_envGrid.velfieldZ);
	float maxVel = 1.0f;
	hipMemcpyAsync(&maxVel, m_singleStor0, sizeof(float), hipMemcpyDeviceToHost, simStream); // Causing long stall due to GPU work catching up to this point.


	const float C = (maxVel + 1e-6f) * dt / m_params.voxelSize;
	const float MAXSUBSTEPS = 100;

	const int subSteps = int(fminf(ceilf(fabsf(C)), MAXSUBSTEPS));
	const float dtSub = dt / (subSteps + 1e-32f);
	if (C > 25)
	{
		printf("WARNING: Courant number is high: %f, increase size of voxels or decrease timesteps, %f, %i\n", C, dtSub, subSteps);
	}

	// Now use single storage as dt holder
	hipMemcpyAsync(m_singleStor0, &dtSub, sizeof(float), hipMemcpyHostToDevice, simStream);
    hipStreamSynchronize(simStream); // Synchronize host copy

	// Check if graph is initialized
    initAdvectPPMWGPUGraph(advectArray, defaultVal, boundsVal);


	for (int i = 0; i < subSteps; i++)
	{
		// Now actually launch the graph we created
		hipGraphLaunch(reinterpret_cast<hipGraphExec_t>(m_advectExecutionGraphs[advectArray]), simStream);
	}

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, advectPPMZ);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);
	//cudaDeviceSynchronize();

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {

		std::cerr << "CUDA error: " << hipGetErrorString(err) << std::endl;
		assert(false);
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

void environmentGPU::initAdvectPPMWGPUGraph(float* advectArray, const float* defaultVal, boundsEnv boundsVal)
{
    if (m_advectExecutionGraphs.find(advectArray) == m_advectExecutionGraphs.end())
	{
		int3 size = make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ);
		const int sharedDataSize = (m_blockDim.x + 2) * (m_blockDim.y + 2) * sizeof(float);




        // Use graphs to get rid of the overhaul between kernel launches
        // We only need to initialize the graph ones, afterwards we can reuse the graph.
        // We do need a different graph for each different array, thus we use a map
		hipGraph_t graph;

		hipStreamBeginCapture(simStream, hipStreamCaptureModeGlobal);

		// We advect the default value and density. 
		// After each time we advect, we need to divide by the density, this is to make sure any errors that accumulate are immediately resolved.
		// Using half x, half y and full z, we use second order accuracy, making sure we treat x, y and z equally. 

		// Using strang method
		// Also used in flash https://flash.rochester.edu/site/flashcode/user_support/flash2_users_guide/docs/FLASH2.5/flash2_ug.pdf
		// in 6.1.3, strang is done using X, Y, Z, then another timestep for Z, Y, X. Combining that in 1 timestep, we can do:
		// 0.5X, 0.5Y, Z, 0.5Y, 0.5X


		// Its kernel time, advecting 2 times half X, 2 times half Y and 1 time Z

		//------------------------------ 0.5 X -----------------------------------

		//First advect density and array on half X
		advectPPMX <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldX, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		advectPPMX <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		//cudaDeviceSynchronize();

		//Divide to get them both up to speed
		divideValuesFull <<<m_gridDim, m_blockDim, 0, simStream >>> (size, m_outputArray, m_stor0);
		setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_density, 1.0f, m_params.sizeZ, 0);
		//cudaDeviceSynchronize();

		//------------------------------ 0.5 Y -----------------------------------

		//Switch blocks and threads around (x and y are swapped)
		//Also input and output are swapped because of previous result
		advectPPMY <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldY, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		advectPPMY <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		//cudaDeviceSynchronize();

		//Divide to get them both up to speed
		divideValuesFull <<<m_gridDim, m_blockDim, 0, simStream >>> (size, advectArray, m_stor0);
		setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_density, 1.0f, m_params.sizeZ, 0);
		//cudaDeviceSynchronize();

		//------------------------------ 1.0 Z -----------------------------------

		advectPPMZ <<<m_gridDim, m_blockDim, 0, simStream >>> (m_params, m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldZ, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		advectPPMZ <<<m_gridDim, m_blockDim, 0, simStream >>> (m_params, advectArray, m_outputArray, defaultVal, m_envGrid.velfieldZ, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		//cudaDeviceSynchronize();

		divideValuesFull <<<m_gridDim, m_blockDim, 0, simStream >>> (size, m_outputArray, m_stor0);
		setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_density, 1.0f, m_params.sizeZ, 0);
		//cudaDeviceSynchronize();

		//------------------------------ 0.5 Y -----------------------------------

		advectPPMY <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldY, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		advectPPMY <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_outputArray, advectArray, defaultVal, m_envGrid.velfieldY, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		//cudaDeviceSynchronize();

		divideValuesFull<<<m_gridDim, m_blockDim, 0, simStream>>>(size, advectArray, m_stor0);
		setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_density, 1.0f, m_params.sizeZ, 0);
		//cudaDeviceSynchronize();

		//------------------------------ 0.5 X -----------------------------------

		advectPPMX <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_density, m_stor0, m_dummyArraySky2, m_envGrid.velfieldX, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		advectPPMX <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, advectArray, m_outputArray, defaultVal, m_envGrid.velfieldX, m_neighbourData, m_GHeight, boundsVal, m_singleStor0);
		//cudaDeviceSynchronize();

		divideValuesFull<<<m_gridDim, m_blockDim, 0, simStream>>>(size, m_outputArray, m_stor0);
		setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_density, 1.0f, m_params.sizeZ, 0);

		//Switch over input and output for our next iteration or return result.
		hipMemcpyAsync(advectArray, m_outputArray, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream);


		hipGraphExec_t newGraphExec;
		hipStreamEndCapture(simStream, &graph);
		hipGraphInstantiateWithFlags(&newGraphExec, graph, 0);
		hipGraphDestroy(graph);
		m_advectExecutionGraphs[advectArray] = reinterpret_cast<hipGraphExec*>(newGraphExec); // Add new graph
	}
}

void environmentGPU::advectPrecip(float* array, const int fallVelType, const float dt)
{
	// Substeps based on highest courant number
	// This is to make sure that the advecting value can not pass over 1 cell, for example:
	// If velocity is too high, the value actually never passes this cell in one time frame but passes over.
	// To fix this we use sub-steps.

	//Guesses since we don't have access to speeds
	float maxVel = fallVelType == 0 ? 5.0f : (fallVelType == 1 ? 1.0f : 8.0f);


	const float C = maxVel * dt / m_params.voxelSize;
	const float MAXSUBSTEPS = 100;
	const int subSteps = int(fminf(ceilf(fabsf(C)), MAXSUBSTEPS));
	const float dtSub = dt / subSteps;

	//Advect precip for the substeps.
	for (int i = 0; i < subSteps; i++)
	{
		// Psst, we swapped the X and Y around since we are only interested in the Y values per block
		advectPrecipGPU<<<m_gridDim, m_blockDim, 0, simStream >>>(m_params, m_GHeight, array, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.pressure, m_groundGrid.P, fallVelType, dtSub);
		//cudaDeviceSynchronize();
	}
}

void environmentGPU::pressureProject(const float dt)
{
	const int sharedDataSize = (m_blockDim.x + 2) * (m_blockDim.y + 2) * sizeof(float);

	int3 size = make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ);
	setToValue <<<1, m_params.sizeY, 0, simStream >>> (size, m_dummyArraySky2, 1.0f, 1, 0);

	//Should not be needed if handled correctly in advection and other velocity updates?
	resetVelPressProj <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ);
	//cudaDeviceSynchronize();

	//Initialize A and precon every frame due to changed based on density
	initAMatrix <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_GHeight, m_A, m_neighbourData, m_densityAir, m_dummyArraySky2);
	//cudaDeviceSynchronize();

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, initAMatrix);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {

		std::cerr << "CUDA error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}


	initPrecon <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_precon, m_A);
	//cudaDeviceSynchronize();

	//Actual pressure project
	calculatePressureProject(dt);

	//Set new density
	updatePressure <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.pressure, m_outputArray);

	initDensity <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_densityAir, m_envGrid.potTemp, m_envGrid.pressure, m_envGrid.Qv, m_groundGrid.P);
	//cudaDeviceSynchronize();


	//Debug
	//Game.Editor().setDebugValueNum(m_outputArray, 2);
	//cudaDeviceSynchronize();

	//Apply calculate pressure to velocity field
	applyPresProjGPU <<<m_gridDim, m_blockDim, 0, simStream >>> (m_params, m_GHeight, m_outputArray, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ,
		m_densityAir, m_envGrid.pressure, dt, m_stor0);
	//cudaDeviceSynchronize();

	//Game.Editor().setDebugValueNum(m_stor0, 1);

	err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

void environmentGPU::calculatePressureProject(const float )
{	
	//const float tolValue = 1e-5f;
	const int MAXITERATION = 50;
	float maxr = 0.0f;
	const int sharedDataSize = (m_blockDim.x + 2) * (m_blockDim.y + 2) * sizeof(float);
	const int sharedDataSizeNoHalo = m_blockDim.x * m_blockDim.y * sizeof(float);

	//m_stor0 = divergence
	//m_stor1 = z
	//m_stor2 = s
	//Reset values
	hipMemsetAsync(m_stor0, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_stor1, 0, m_params.size * sizeof(float), simStream);
	hipMemsetAsync(m_stor2, 0, m_params.size * sizeof(float), simStream);
	// If ground changed we do not want to use previous guess since that is all wrong
	if (m_groundChanged) hipMemsetAsync(m_outputArray, 0, m_params.size * sizeof(float), simStream);
	else hipMemcpyAsync(m_outputArray, m_storPres, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream); // Starting with previous result
	hipMemsetAsync(m_singleStor0, 0, sizeof(float), simStream);
	hipMemsetAsync(m_sigma0, 0, sizeof(float), simStream);
	hipMemsetAsync(m_sigma1, 0, sizeof(float), simStream);

	hipStreamSynchronize(simStream);

	//Divergence
	calculateDivergenceGPU <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_GHeight, m_stor0, m_neighbourData, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_densityAir, m_oldDensityAir, m_dummyArraySky2);
	//cudaDeviceSynchronize();
	

	if (!m_groundChanged)
	{
		// Set initial residual vector guess r = d - Ap
		applyAGPU <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_stor1, m_storPres, m_neighbourData, m_A);
		subtractArrayFull <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ), m_stor0, m_stor1);
	}

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, calculateDivergenceGPU);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {

		std::cerr << "CUDA error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	//Debug
	//cudaDeviceSynchronize();
	//Game.Editor().setDebugValueNum(m_stor0, 2, simStream);

	//Check max divergence
	getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo, simStream >>> (m_params, m_GHeight, m_singleStor0, m_stor0);
	//cudaDeviceSynchronize();
	hipMemcpyAsync(&maxr, m_singleStor0, sizeof(float), hipMemcpyDeviceToHost, simStream);
	hipStreamSynchronize(simStream); // Synchronize host copy
	if (maxr == 0.0f) return;
	//printf("init r value: %e\n", maxr);

	applyPreconditionerGPU <<<m_gridDim, m_blockDim, 0, simStream >>> (m_params, m_GHeight, m_stor1, m_precon, m_stor0, m_A);
	//cudaDeviceSynchronize();


	hipMemcpyAsync(m_stor2, m_stor1, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream);
	dotProductGPU <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo, simStream >>> (m_params, m_GHeight, m_sigma0, m_stor1, m_stor0);
	//cudaDeviceSynchronize();

	// Check if we need to initialize the graph
	initPressureProjectGraph();


	for (int i = 0; i < MAXITERATION; i++) // TODO: MAXITERATION
	{
		hipGraphLaunch(reinterpret_cast<hipGraphExec_t>(m_pressureprojectExecutionGraph), simStream);

		//// Same bounds as density since A = density
		//applyAGPU<<<m_gridDim, m_blockDim, sharedDataSize >>>(m_stor1, m_stor2, m_neighbourData, m_A);
		////cudaDeviceSynchronize();

		//dotProductGPU <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo >>> (m_sigma1, m_stor1, m_stor2);
		////cudaDeviceSynchronize();

		////Update the pressure (output) and divergence
		//updatePandDiv <<<m_gridDim, m_blockDim >>> (m_sigma0, m_sigma1, m_outputArray, m_stor0, m_stor2, m_stor1);
		////cudaDeviceSynchronize();

		////Check max divergence
		//cudaMemset(m_singleStor0, 0, sizeof(float));
		//getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo >>> (m_singleStor0, m_stor0);
		////cudaDeviceSynchronize();
		////cudaMemcpy(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost);
		////if (maxr <= tolValue)
		////{
		////	printf("Iterations pressure projection: %i\n", i);
		////	return;
		////}

		//applyPreconditionerGPU <<<m_gridDim, m_blockDim >>> (m_stor1, m_precon, m_stor0, m_A);

		////cudaDeviceSynchronize();

		////Dotproduct
		//cudaMemset(m_sigma1, 0, sizeof(float));
		//dotProductGPU <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo >>> (m_sigma1, m_stor1, m_stor0);
		////cudaDeviceSynchronize();

		////Set values and set search vector
		//endIteration <<<m_gridDim, m_blockDim >>> (m_sigma0, m_sigma1, m_stor2, m_stor1);

		//cudaMemset(m_sigma1, 0, sizeof(float));

		//cudaDeviceSynchronize();
	}

	// Check r
	//cudaMemsetAsync(m_singleStor0, 0, sizeof(float), stream1);
	//getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo, stream1 >>> (m_singleStor0, m_stor0);
	//////cudaDeviceSynchronize();
	//cudaMemcpyAsync(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost, stream1);
	//printf("r value: %e\n", maxr);


	//Game.Editor().setDebugValueNum(m_outputArray, 1);

	// Set our next initial pressure guess. 
	hipMemcpyAsync(m_storPres, m_outputArray, m_params.size * sizeof(float), hipMemcpyDeviceToDevice, simStream);
	
	//printf("Max iterations reached!\n");
}

void environmentGPU::initPressureProjectGraph() 
{
    if (!m_pressureprojectExecutionGraph)
    {
        const int sharedDataSize = (m_blockDim.x + 2) * (m_blockDim.y + 2) * sizeof(float);
        const int sharedDataSizeNoHalo = m_blockDim.x * m_blockDim.y * sizeof(float);

		// Use graphs to get rid of the overhaul between kernel launches
        // We only need to initialize the graph ones, afterwards we can reuse the graph.
        hipGraph_t graph;
        hipStreamBeginCapture(simStream, hipStreamCaptureModeGlobal);

        applyAGPU<<<m_gridDim, m_blockDim, sharedDataSize, simStream>>>(m_params, m_stor1, m_stor2, m_neighbourData, m_A);
        dotProductGPU<<<m_gridDim, m_blockDim, sharedDataSizeNoHalo, simStream>>>(m_params, m_GHeight, m_sigma1, m_stor1, m_stor2);
        updatePandDiv<<<m_gridDim, m_blockDim, 0, simStream>>>(m_params, m_GHeight, m_sigma0, m_sigma1, m_outputArray, m_stor0, m_stor2, m_stor1);

        // cudaMemsetAsync(m_singleStor0, 0, sizeof(float), simStream);
        // getMaxDivergence <<<m_gridDim, m_blockDim, sharedDataSizeNoHalo, simStream >>> (m_singleStor0, m_stor0);
        ////cudaDeviceSynchronize();
        // cudaMemcpyAsync(&maxr, m_singleStor0, sizeof(float), cudaMemcpyDeviceToHost, simStream);
        // if (maxr <= tolValue)
        //{
        //	printf("Iterations pressure projection lower than max\n");
        //	//return;
        // }

        applyPreconditionerGPU<<<m_gridDim, m_blockDim, 0, simStream>>>(m_params, m_GHeight, m_stor1, m_precon, m_stor0, m_A);
        hipMemsetAsync(m_sigma1, 0, sizeof(float), simStream);
        dotProductGPU<<<m_gridDim, m_blockDim, sharedDataSizeNoHalo, simStream>>>(m_params, m_GHeight, m_sigma1, m_stor1, m_stor0);
        endIteration<<<m_gridDim, m_blockDim, 0, simStream>>>(m_params, m_GHeight, m_sigma0, m_sigma1, m_stor2, m_stor1);
        hipMemsetAsync(m_sigma1, 0, sizeof(float), simStream);

        hipStreamEndCapture(simStream, &graph);
        hipGraphInstantiateWithFlags(reinterpret_cast<hipGraphExec_t*>(&m_pressureprojectExecutionGraph), graph, 0);
        hipGraphDestroy(graph);
    }
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
	calculateCloudCoverGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_dummyArrayGround, m_envGrid.Qc, m_envGrid.Qw, m_GHeight);
}

void environmentGPU::updateGroundTemps(const float dt, const float speed, const float irridiance)
{
	calculateGroundTempGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_groundGrid.T, dt * speed, irridiance, m_dummyArrayGround);
}

void environmentGPU::calculateBuoyancy(const float )
{
	// Neumann, since we will just use the current temp but divide by 2, meaning we don't actually use outside or ground
	const int sharedDataSize = (m_blockDim.x + 2) * (m_blockDim.y + 2) * sizeof(float) * 2;

	buoyancyGPU <<<m_gridDim, m_blockDim, sharedDataSize, simStream >>> (m_params, m_GHeight, m_envGrid.velfieldY, m_neighbourData, m_envGrid.potTemp, m_envGrid.Qv, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi, m_isentropicTemp, m_isentropicVapor, m_envGrid.pressure, m_groundGrid.P, m_stor0);
	hipStreamSynchronize(simStream);
	//Set debug
     m_debugArray1 = m_stor0;

	//cudaFuncAttributes attr;
	//cudaFuncGetAttributes(&attr, buoyancyGPU);
	//printf("Registers used in kernel per thread: %d\n", attr.numRegs);


	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {

		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

bool environmentGPU::isGround(int x, int y)
{
	return y <= m_GHeight[x];
}

float* environmentGPU::getParamArray(parameter type)
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
	case WINDX:
		return m_envGrid.velfieldX;
		break;
	case WINDY:
		return m_envGrid.velfieldY;
		break;
	case WINDZ:
		return m_envGrid.velfieldZ;
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

void environmentGPU::updateOutOfSyncGround()
{
	//First set all data back that was in the ground before
	compareAndResetValuesOutGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_dummyGHeight, m_isentropicTemp, m_isentropicVapor,
		m_envGrid.Qv, m_envGrid.Qw, m_envGrid.Qc, m_envGrid.Qr, m_envGrid.Qs, m_envGrid.Qi,
		m_envGrid.potTemp, m_envGrid.velfieldX, m_envGrid.velfieldY, m_envGrid.velfieldZ, m_envGrid.pressure, m_defaultPressure);

	//Then set the groundheight correct
	hipMemcpyAsync(m_GHeight, m_dummyGHeight, m_params.sizeGround * sizeof(int), hipMemcpyDeviceToDevice, simStream);

	//Update GPU values
	resetGroundValues();

	hipStreamSynchronize(simStream);

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}

	m_groundChanged = true;
	m_updateGround = false;
}

void environmentGPU::resetGroundValues()
{
	//Reset values that are in ground
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.potTemp, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qv, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qw, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qc, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qr, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qs, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.Qi, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.velfieldX, m_GHeight);
	resetValueInGround <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_envGrid.velfieldY, m_GHeight);
	hipStreamSynchronize(simStream);

	computeNeighbourGPU <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_neighbourData);
	computeIsenTempGroundGPU <<<m_params.sizeZ, m_params.sizeX, 0, simStream >>> (m_params, m_groundGrid.T, m_isentropicTemp, m_groundGrid.P, m_envGrid.pressure, m_GHeight);
}

void environmentGPU::prepareBrushGPU(parameter paramType, const float brushSize, const glm::ivec3 mousePos, const float brushSmoothnes, const float dt, const float brushIntensity, const float applyValue, const glm::vec3 valueDir, const bool groundErase)
{
	hipMemcpyAsync(m_dummyGHeight, m_GHeight, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToDevice, simStream);
	bool changedGround = false;
	hipMemsetAsync(m_storBool, 0, sizeof(bool), simStream);

	int blocks = int(std::ceil(brushSize)) - int(std::floor(-brushSize));
	int threads = blocks;

	//Get correct array to possibly change
	float* array = nullptr;
	float* array2 = nullptr;
	float* array3 = nullptr;
	if (paramType == WINDX || paramType == WINDY || paramType == WINDZ)
	{
		array = getParamArray(WINDX);
		array2 = getParamArray(WINDY);
		array3 = getParamArray(WINDZ);
	}
	else if (paramType != PGROUND)
	{
		array = getParamArray(paramType);
	}
	//Actually calculate where to put brush and apply brush values
	float3 valueDirGPU = make_float3(valueDir.x, valueDir.y, valueDir.z);
	int3 mousePosGPU = make_int3(mousePos.x, mousePos.y, mousePos.z);
	applyBrushGPU <<<blocks, threads, 0, simStream >>> (m_params, m_GHeight, array, array2, array3, m_dummyGHeight, m_storBool, paramType, brushSize, mousePosGPU, brushSmoothnes, brushIntensity, applyValue, valueDirGPU, groundErase, dt);
	hipStreamSynchronize(simStream);

	if (!m_updatingSimulation)
    {
		// If not simulating, update the renderer ourself
        // TODO, update rendering data
    }

	//Check for ground changed
	hipMemcpyAsync(&changedGround, m_storBool, sizeof(bool), hipMemcpyDeviceToHost, simStream);
	if (changedGround)
	{
		lockGlobal();
		if (!m_updatingSimulation)
		{
			updateOutOfSyncGround();
		}
		else m_updateGround = true; // Notify for next iteration to updateInput ground
		// If done immediately, it could crash the simulation
		unlockGlobal();
	}

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

void environmentGPU::prepareSelectionGPU(parameter paramType, const glm::ivec3 minPos, const glm::ivec3 maxPos, const float applyValue, const glm::vec3 valueDir, const bool groundErase)
{
	hipMemcpyAsync(m_dummyGHeight, m_GHeight, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToDevice, simStream);
	bool changedGround = false;
	hipMemsetAsync(m_storBool, 0, sizeof(bool), simStream);

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
	if (paramType == WINDX || paramType == WINDY || paramType == WINDZ)
	{
		array = getParamArray(WINDX);
		array2 = getParamArray(WINDY);
		array3 = getParamArray(WINDZ);
	}
	else if (paramType != PGROUND)
	{
		array = getParamArray(paramType);
	}
	// Now actually apply the selection to the grid
	float3 valueDirGPU = make_float3(valueDir.x, valueDir.y, valueDir.z);
	int3 minPosGPU = make_int3(minPos.x, minPos.y, minPos.z);
	int3 maxPosGPU = make_int3(maxPos.x, maxPos.y, maxPos.z);
	applySelectionGPU <<<blocks, threads, 0, simStream >>> (m_params, m_GHeight, array, array2, array3, m_dummyGHeight, m_storBool, paramType, minPosGPU, maxPosGPU, applyValue, valueDirGPU, groundErase);

	if (!m_updatingSimulation)
    {
		// If not simulating, update the renderer ourself
		// TODO, update rendering data
	}

	//Check for ground changed
	hipMemcpyAsync(&changedGround, m_storBool, sizeof(bool), hipMemcpyDeviceToHost, simStream);
	if (changedGround)
	{
		lockGlobal();
		if (!m_updatingSimulation)
		{
			updateOutOfSyncGround();
		}
		else m_updateGround = true; // Notify for next iteration to updateInput ground
		unlockGlobal();
	}

	hipError_t err = hipGetLastError();
	if (err != hipSuccess) {
		std::cerr << "Cuda error: " << hipGetErrorString(err) << std::endl;
		assert(false);
	}
}

void environmentGPU::resetParameterGPU(parameter paramType)
{
	int3 size = make_int3(m_params.sizeX, m_params.sizeY, m_params.sizeZ);

	if (paramType == POTTEMP) setToDefault <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.potTemp, m_isentropicTemp);
	if (paramType == WINDX || paramType == WINDY || paramType == WINDZ)
	{
		setToDefault <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.velfieldX, m_defaultVelX);
		setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.velfieldY, 0.0f, m_params.sizeZ, 0);
		setToDefault <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.velfieldZ, m_defaultVelZ);
	}
	if (paramType == PRESSURE)
	{
		setToDefault <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.pressure, m_defaultPressure);
	}
	if (paramType == QV) setToDefault <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (m_params, m_GHeight, m_envGrid.Qv, m_isentropicVapor);
	if (paramType == QW) setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.Qw, 0.0f, m_params.sizeZ, 0);
	if (paramType == QC) setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.Qc, 0.0f, m_params.sizeZ, 0);
	if (paramType == QR) setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.Qr, 0.0f, m_params.sizeZ, 0);
	if (paramType == QS) setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.Qs, 0.0f, m_params.sizeZ, 0);
	if (paramType == QI) setToValue <<<m_params.sizeY, m_params.sizeX, 0, simStream >>> (size, m_envGrid.Qi, 0.0f, m_params.sizeZ, 0);

	if (!m_updatingSimulation)
    {
		// If not simulating, update the renderer ourself
		// TODO, update rendering data
	}
}

void environmentGPU::setHostData(envDebugData& outputCPUData, bool setIsentropics)
{ 
	lockGlobal();
	// Copy over GPU data to our little CPU friend
        

    // Set sky values.
    hipMemcpyAsync(outputCPUData.m_envView.potTemp, m_envGrid.potTemp, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.Qv, m_envGrid.Qv, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.Qw, m_envGrid.Qw, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.Qc, m_envGrid.Qc, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.Qr, m_envGrid.Qr, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.Qs, m_envGrid.Qs, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.Qi, m_envGrid.Qi, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.velFieldX, m_envGrid.velfieldX, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.velFieldY, m_envGrid.velfieldY, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_envView.velFieldZ, m_envGrid.velfieldZ, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);

    // Set pressure
    hipMemcpyAsync(outputCPUData.m_envView.pressure, m_envGrid.pressure, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);

    // Set Ground values
    hipMemcpyAsync(outputCPUData.m_groundView.T, m_groundGrid.T, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundView.Qrs, m_groundGrid.Qrs, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundView.Qgr, m_groundGrid.Qgr, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundView.Qgs, m_groundGrid.Qgs, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundView.Qgi, m_groundGrid.Qgi, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundView.P, m_groundGrid.P, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundView.t, m_groundGrid.t, m_params.sizeGround * sizeof(float), hipMemcpyDeviceToHost, simStream);
    hipMemcpyAsync(outputCPUData.m_groundHeight, m_GHeight, m_params.sizeGround * sizeof(int), hipMemcpyDeviceToHost, simStream);


	if (setIsentropics)
    {
        hipMemcpyAsync(outputCPUData.m_envTemp, m_isentropicTemp, m_params.sizeY * sizeof(float), hipMemcpyDeviceToHost, simStream);
        hipMemcpyAsync(outputCPUData.m_envVapor, m_isentropicVapor, m_params.sizeY * sizeof(float), hipMemcpyDeviceToHost, simStream); 
        hipMemcpyAsync(outputCPUData.m_envPressure, m_defaultPressure, m_params.sizeY * sizeof(float), hipMemcpyDeviceToHost, simStream); 
    }

	unlockGlobal();

	hipError_t err = hipGetLastError();
	if (err != hipSuccess)
	{
	    std::cerr << "error: " << hipGetErrorString(err) << std::endl;
	    assert(false);
	}
}

void environmentGPU::setHostSettingsData(float& time, int day, float sunStrength, float longitude, bool pauseDiurnal) 
{
	// If time < 0, we have not updated it in editor, so we retrieve from environmnemt
    if (time < 0) time = m_time;
    else m_time = time;

	m_day = day;
    m_sunStrength = sunStrength;
    m_longitude = longitude;
    m_pauseDiurnal = pauseDiurnal;
}

void environmentGPU::setMicroPhysDataValues(glm::ivec3 minPos, glm::ivec3 maxPos, bool active)
{
	m_microPhysMinPos = minPos;
    m_microPhysMaxPos = maxPos;
	m_microPhysDataActive = active;
}

void environmentGPU::retrieveMicroPhysResults(microPhysEnvValuesSingle& singleParamsCPU)
{
	// Compile-time check to see if it is legal to copy
	static_assert(std::is_trivially_copyable_v<microPhysicsParams>);
	static_assert(std::is_standard_layout_v<microPhysicsParams>);
	static_assert(sizeof(microPhysicsParams) == sizeof(microPhysEnvValuesSingle), "Struct microPhysicsParams and struct microPhysEnvValuesSingle are not the same");
	static_assert(offsetof(microPhysicsParams, PVSUB) == offsetof(microPhysEnvValuesSingle, PVSUB), "Struct microPhysicsParams and struct microPhysEnvValuesSingle have different offsets");

	// Copy GPU to CPU data
	hipMemcpyAsync(&singleParamsCPU, m_microPhysRes, sizeof(microPhysicsParams), hipMemcpyDeviceToHost, simStream);
}

void environmentGPU::getDebugArrayValues(envDebugData& outputCPUData) 
{
    if (m_debugArray0) hipMemcpyAsync(outputCPUData.m_debugArray0, m_debugArray0, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    if (m_debugArray1) hipMemcpyAsync(outputCPUData.m_debugArray1, m_debugArray1, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
    if (m_debugArray2) hipMemcpyAsync(outputCPUData.m_debugArray2, m_debugArray2, m_params.size * sizeof(float), hipMemcpyDeviceToHost, simStream);
}

void environmentGPU::synchronizeSimStream()
{
	hipStreamSynchronize(simStream);
}