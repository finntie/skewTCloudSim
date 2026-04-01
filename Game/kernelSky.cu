#include "kernelSky.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "utils.cuh"
#include "environment.cuh"
#include "environment.h"
#include "meteoconstants.cuh"
#include "meteoformulas.cuh"
#include "editor.h"

__constant__ int GHeight[GRIDSIZEGROUND];
__constant__ float defaultVelX[GRIDSIZESKYY];
__constant__ float defaultVelZ[GRIDSIZESKYY];

__constant__ float gammaR;
__constant__ float gammaS;
__constant__ float gammaI;

void initKernelSky(const int* _GHeight, const float* _defaultVelX, const float* _defaultVelZ)
{
	//Set constant data for easier access
	cudaMemcpyToSymbol(GHeight, _GHeight, GRIDSIZEGROUND * sizeof(int));
	cudaMemcpyToSymbol(defaultVelX, _defaultVelX, GRIDSIZESKYY * sizeof(float));
	cudaMemcpyToSymbol(defaultVelZ, _defaultVelZ, GRIDSIZESKYY * sizeof(float));

	const float b = 0.8f;
	const float d = 0.25f;
	float GammaR = tgammaf(4.0f + b);
	float GammaS = tgammaf(4.0f + d);
	float GammaI = GammaS;
	cudaMemcpyToSymbol(gammaR, &GammaR, sizeof(float));
	cudaMemcpyToSymbol(gammaS, &GammaS, sizeof(float));
	cudaMemcpyToSymbol(gammaI, &GammaI, sizeof(float));
}

//-------------------------------------DIFFUSION-------------------------------------


__global__ void diffuseRedBlack(const Neigh* neigh, const float* groundT, const float* pressuresAir, const float* groundP, const float* defaultVal,
	const float* input, float* output, const float k, const int type, boundsEnv bounds, bool red)
{
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	// Make sure to not access nullptr
	float defaultValue = defaultVal ? defaultVal[y] : 0.0f;
	// Already set forward and current to the position where they can get easily swapped in the for loop
	float forward = input[idx];
	float current = 0.0f;
	current = fillNeighbourData(neigh[idx].backward, bounds, input, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultValue);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();
		idx = getIdx(x, y, z);

		backward = current;
		current = forward;
		forward = fillNeighbourData(neigh[idx].forward, bounds, input, idx, GRIDSIZESKYX * GRIDSIZESKYY, defaultValue);


		fillSharedNeigh(sharedBlock, input, defaultVal, z, bounds);
		__syncthreads();


		float l = 0.0f, r = 0.0f, d = 0.0f, u = 0.0f, f = 0.0f, b = 0.0f;

		// We do not return, since we still need to fill data
		if ((x + y + z) % 2 == static_cast<int>(red) || isGroundGPU(x, y, z)) continue;

		// We use custom boundary conditions on ground for temperature,
		// This is because we do not save potential temp on the ground
		if (type == 0)
		{
			const int idxG = x + z * GRIDSIZESKYX;
			const int idxGL = x > 0 ? idxG - 1 : idxG;
			const int idxGR = x + 1 < GRIDSIZESKYX ? idxG + 1 : idxG;
			const int idxGF = z + GRIDSIZESKYX < GRIDSIZESKYZ ? idxG + GRIDSIZESKYX : idxG;
			const int idxGB = z > 0 ? idxG - GRIDSIZESKYX : idxG;


			l = (neigh[idx].left == GROUND && isGroundLevel(z)) ? (potentialTempGPU(groundT[idxGL] - 273.15f, pressuresAir[idx], groundP[idxGL]) + 273.15f) : sharedBlock[idxsData - 1];
			r = (neigh[idx].right == GROUND && isGroundLevel(z) ? (potentialTempGPU(groundT[idxGR] - 273.15f, pressuresAir[idx], groundP[idxGR]) + 273.15f) : sharedBlock[idxsData + 1]);
			d = (neigh[idx].down == GROUND) ? potentialTempGPU(groundT[idxG] - 273.15f, pressuresAir[idx], groundP[idxG]) + 273.15f : sharedBlock[idxsData - sharedBlockWidth];
			u = sharedBlock[idxsData + sharedBlockWidth]; // No ground upwards, so we can just use the shared block
			f = (neigh[idx].forward == GROUND && isGroundLevel(z)) ? (potentialTempGPU(groundT[idxGF] - 273.15f, pressuresAir[idx], groundP[idxGF]) + 273.15f) : forward;
			b = (neigh[idx].backward == GROUND && isGroundLevel(z)) ? (potentialTempGPU(groundT[idxGB] - 273.15f, pressuresAir[idx], groundP[idxGB]) + 273.15f) : backward;
			
		}
		else
		{
			l = sharedBlock[idxsData - 1];
			r = sharedBlock[idxsData + 1];
			d = sharedBlock[idxsData - sharedBlockWidth];
			u = sharedBlock[idxsData + sharedBlockWidth];
			f = forward;
			b = backward;
		}

		output[idx] = (input[idx] + k * (l + r + u + d + f + b)) / (1 + 6 * k);
	}
}

//-------------------------------------ADVECTION-------------------------------------


__global__ void advectGroundWaterGPU(float* Qrs, float* Qgr, const float dt, const float speed)
{
	// 17 x 17 since we only look at previous not ahead
	__shared__ float sharedBlockQrs[17 * 17];
	__shared__ float sharedBlockQgr[17 * 17];

	const int sharedBlockWidth = blockDim.x + 1;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int z = threadIdx.y + blockDim.y * blockIdx.y;
	const int idx = x + z * GRIDSIZESKYX;
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;
	int idxL = idx - 1;
	int idxB = idx - GRIDSIZESKYX;

	// Filling shared data
	const float rsC = Qrs[idx];
	const float grC = Qgr[idx];
	sharedBlockQrs[idxsData] = rsC;
	sharedBlockQgr[idxsData] = grC;

	if (x == 0)
	{
		sharedBlockQrs[idxsData - 1] = rsC;
		sharedBlockQgr[idxsData - 1] = grC;
		idxL = idx;
	}
	if (z == 0)
	{
		sharedBlockQrs[idxsData - sharedBlockWidth] = rsC;
		sharedBlockQgr[idxsData - sharedBlockWidth] = grC;
		idxB = idx;
	}

	// Now, hold up
	__syncthreads();


	// Set neighbour values
	float rsL = 0.0f, rsB = 0.0f;
	float grL = 0.0f, grB = 0.0f;

	rsL = sharedBlockQrs[idxsData - 1];
	rsB = sharedBlockQrs[idxsData - sharedBlockWidth];

	grL = sharedBlockQgr[idxsData - 1];
	grB = sharedBlockQgr[idxsData - sharedBlockWidth];
	
	//Check how many meters down
	float slopeFlowRainX = 0.0f;
	float slopeFlowRainZ = 0.0f;

	float slopeFlowWaterX = 0.0f;
	float slopeFlowWaterZ = 0.0f;
	{
		// Using Manning Formula: V = 1 / n * Rh^2/3 * S^1/2
		// We use for n = 0.030.
		// RH is weird for open way, so we use water depth, which is in our case is Qgr * 1 (in meters)
		const float n = 1 / 0.03f;

		// Hydraulic conductivity of the ground in m/s (How easy water flows through ground) https://structx.com/Soil_Properties_007.html
		const float k{ 5e-5f }; //Sand	

		// Divide by 1.0f is useless but its to say that height changed with n by 1 meters.
		const float slopeX = GHeight[idxL] - GHeight[idx] / 1.0f;
		const float slopeZ = GHeight[idxB] - GHeight[idx] / 1.0f;

		const bool fromPrevX = slopeX > 0.0f;
		const bool fromPrevZ = slopeZ > 0.0f;
		const float slopeFromRainX = fromPrevX ? grL : grC;
		const float slopeFromWaterX = fromPrevX ? rsL : rsC;
		const float slopeFromRainZ = fromPrevZ ? grB : grC;
		const float slopeFromWaterZ = fromPrevZ ? rsB : rsC;

		float changeRainX = 0.0f;
		float changeRainZ = 0.0f;

		if (slopeFromRainX != 0.0f || slopeFromRainZ != 0.0f)
		{
			// Height of water
			const float RHX = slopeFromRainX;
			const float RHZ = slopeFromRainZ;
			const float speedX = n * powf(RHX, 2.0f / 3.0f);
			const float speedZ = n * powf(RHZ, 2.0f / 3.0f);

			// In m/s
			const float velX = speedX * pow(abs(slopeX), 0.5f);
			const float velZ = speedZ * pow(abs(slopeZ), 0.5f);

			// Speed * time = distance, time will be included after limiting
			// Multiplying by slopeFromRain makes the water kind of stick the less it is, which is okay
			changeRainX = velX * slopeFromRainX;
			changeRainZ = velZ * slopeFromRainZ;

		}
		const float changeWaterX = ConstantsGPU::g * k * slopeFromWaterX * slopeX;
		const float changeWaterZ = ConstantsGPU::g * k * slopeFromWaterZ * slopeZ;

		//Limit
		slopeFlowRainX = dt * fmin(changeRainX * speed, fromPrevX ? grL : grC);
		slopeFlowRainZ = dt * fmin(changeRainZ * speed, fromPrevZ ? grB : grC);
		slopeFlowWaterX = dt * fmin(changeWaterX * speed, fromPrevX ? rsL : rsC);
		slopeFlowWaterZ = dt * fmin(changeWaterZ * speed, fromPrevZ ? rsB : rsC);
	}

	const float DG = 1.75e-3f; // diffusion coefficient for ground rain water https://dtrx.de/od/diff/
	const float DS = 1.8e-5f; // diffusion coefficient for subsurface water https://www.researchgate.net/figure/Diffusion-coefficient-for-water-in-soils_tbl2_267235072

	const float diffusionR = 1 / (VOXELSIZE * VOXELSIZE) * DG;
	const float diffusionW = 1 / (VOXELSIZE * VOXELSIZE) * DS;

	const float lapRX = (grL - grC)  * diffusionR;
	const float lapRZ = (grB - grC)  * diffusionR;
	const float lapSRX = (rsL - rsC) * diffusionW;
	const float lapSRZ = (rsB - rsC) * diffusionW;

	const bool fromPrevRX = lapRX > 0.0f;
	const bool fromPrevRZ = lapRZ > 0.0f;
	const bool fromPrevSRX = lapSRX > 0.0f;
	const bool fromPrevSRZ = lapSRZ > 0.0f;
	//Limit
	const float flowGroundRainX = dt * fmin(lapRX * speed, fromPrevRX ? grL : grC);
	const float flowGroundRainZ = dt * fmin(lapRZ * speed, fromPrevRZ ? grB : grC);
	const float flowGroundWaterX = dt * fmin(lapSRX * speed, fromPrevSRX ? rsL : rsC);
	const float flowGroundWaterZ = dt * fmin(lapSRZ * speed, fromPrevSRZ ? rsB : rsC);

	// Finally we set the data
	Qgr[idxL] -= flowGroundRainX + slopeFlowRainX;
	Qgr[idxB] -= flowGroundRainZ + slopeFlowRainZ;
	Qrs[idxL] -= flowGroundWaterX + slopeFlowWaterX;
	Qrs[idxB] -= flowGroundWaterZ + slopeFlowWaterZ;
	

	Qgr[idx] += flowGroundRainX + slopeFlowRainX;
	Qgr[idx] += flowGroundRainZ + slopeFlowRainZ;
	Qrs[idx] += flowGroundWaterX + slopeFlowWaterX;
	Qrs[idx] += flowGroundWaterZ + slopeFlowWaterZ;
}

__global__ void advectGroundWaterBlack(const float* inputQrs, const float* inputQgr, float* Qrs, float* Qgr, const float dt, const float speed)
{
	const int tX = threadIdx.x;
	if (tX % 2 == 1 || tX == 0) return; //When 1 we return at black (or at the first one)

	const int pX = tX - 1;

	//Check how many meters down
	float slopeFlowRain = 0.0f;
	float slopeFlowWater = 0.0f;
	{
		// Using Manning Formula: V = 1 / n * Rh^2/3 * S^1/2
		// We use for n = 0.030.
		// RH is weird for open way, so we use water depth, which is in our case is Qgr * VOXELSIZE (in meters)
		const float n = 1 / 0.03f;

		// Hydraulic conductivity of the ground in m/s (How easy water flows through ground) https://structx.com/Soil_Properties_007.html
		const float k{ 5e-5f }; //Sand	

		const float slope = float(GHeight[pX] - GHeight[tX]) / 1.0f; // Divide by 1 is useless but its to say that height changed with n by 1 meters.
		const bool fromPrev = slope > 0.0f;
		const float slopeFromRain = fromPrev ? inputQgr[pX] : inputQgr[tX];
		const float slopeFromWater = fromPrev ? inputQrs[pX] : inputQrs[tX];

		float changeRain = 0.0f;
		if (slopeFlowRain != 0.0f)
		{
			// Height of water
			const float RH = slopeFromRain * VOXELSIZE;
			const float speed1 = n * powf(RH, 2.0f / 3.0f);

			// In m/s
			const float vel = speed1 * pow(abs(slope), 0.5f);

			// Speed * time = distance, time will be included after limiting
			// Multiplying by slopeFromRain makes the water kind of stick the less it is, which is okay
			changeRain = vel * slopeFromRain;
		}
		const float changeWater = ConstantsGPU::g * k * slopeFromWater * slope;

		//Limit
		slopeFlowRain = dt * fmin(changeRain * speed, fromPrev ? inputQgr[pX] : inputQgr[tX]);
		slopeFlowWater = dt * fmin(changeWater * speed, fromPrev ? inputQrs[pX] : inputQrs[tX]);
	}

	const float DG = 1.75e-3f; // diffusion coefficient for ground rain water https://dtrx.de/od/diff/
	const float DS = 1.8e-5f; // diffusion coefficient for subsurface water https://www.researchgate.net/figure/Diffusion-coefficient-for-water-in-soils_tbl2_267235072

	const float lapR = (inputQgr[pX] - inputQgr[tX]) / (VOXELSIZE * VOXELSIZE) * DG;
	const float lapSR = (inputQrs[pX] - inputQrs[tX]) / (VOXELSIZE * VOXELSIZE) * DS;
	const bool fromPrevR = lapR > 0.0f;
	const bool fromPrevSR = lapSR > 0.0f;
	//Limit
	const float flowGroundRain = dt * fmin(lapR * speed, fromPrevR ? inputQgr[pX] : inputQgr[tX]);
	const float flowGroundWater = dt * fmin(lapSR * speed, fromPrevSR ? inputQrs[pX] : inputQrs[tX]);

	Qgr[pX] -= flowGroundRain + slopeFlowRain;
	Qrs[pX] -= flowGroundWater + slopeFlowWater;
	Qgr[tX] += flowGroundRain + slopeFlowRain;
	Qrs[tX] += flowGroundWater + slopeFlowWater;
}

__global__ void setTempsAtGroundGPU(float* potTemps, const float* groundTemps, const float* pressuresAir, const float* groundPressures, const float dt)
{
	const int x = threadIdx.x;
	const int y = GHeight[x] + 1;
	const int z = blockIdx.x;
	const int idx = getIdx(x, y, z);
	const int Gidx = x + z * GRIDSIZESKYX; // Ground index

	if (y >= GRIDSIZESKYY - 1) return;

	const float T = potentialTempGPU(groundTemps[Gidx] - 273.15f, pressuresAir[idx], groundPressures[Gidx]) + 273.15f;
	const float dif2 = potTemps[idx] - T;
	potTemps[idx] -= dif2 * fminf(1.0f, dt);
}

__device__ float advectPPMFlux(const float velocity, const float valL, const float valC, const float valR, const float dt)
{
	const float veli = velocity;

	//Calculate C (Courant number)
	float C = veli * dt / VOXELSIZE;
	//if (fabsf(C) > 1.0f) printf("HOW IS THIS HIGHER THAN 1.0??, %f %f, %f\n", veli, dt, C);
	//We limit, this is not good, but we already do substeps, so just in case to not get an error:
	C = C > 1.0f ? 1.0f : (C < -1.0f ? -1.0f : C);

	const float qMin = cuda::std::fmin(valL, cuda::std::fmin(valC, valR));
	const float qMax = cuda::std::fmax(valL, cuda::std::fmax(valC, valR));

	//Slope limiter: van Leer
	float s = 0.0f;
	const float diffLeft = valC - valL;
	const float diffRight = valR - valC;

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
		const float diffBoth = valR - valL;
		const float absLeft = cuda::std::fabsf(diffLeft);
		const float absRight = cuda::std::fabsf(diffRight);
		const float absBoth = cuda::std::fabsf(diffBoth);

		const float minDiff = cuda::std::fmin(absRight, absLeft);
		const float slopeLimit = cuda::std::fmin(0.5f * absBoth, minDiff);

		//Sign
		s = diffBoth >= 0.0f ? slopeLimit : -slopeLimit;
	}

	//Set initial parabolic edges
	const float qLS = valC - 0.5f * s; //Left edge
	const float qRS = valC + 0.5f * s; //Right edge

	//Monotonicity constrained from Walcek
	float B = 1.0f;

	//If right edge is bigger then max or left edge is smaller then minimum.
	//Can only happen if qi is NOT a local extremum (s = 0.0)
	if (qRS > qMax)
	{
		const float denominator = qRS - valC;
		if (denominator > 1e-16f)
		{
			B = cuda::std::fmin(B, (qMax - valC) / denominator);
		}
	}
	if (qLS < qMin)
	{
		const float denominator = valC - qLS;
		if (denominator > 1e-16f)
		{
			B = cuda::std::fmin(B, (valC - qMin) / denominator);
		}
	}

	//Scale to be in between min and max
	float qR = valC + B * (qRS - valC);
	float qL = valC + B * (qLS - valC);

	//Fix extremum (qi is higher/lower then neighbours) i.e. (1,2,1)
	//We just flatten it...
	if ((qR - valC) * (valC - qL) <= 0.0f)
	{
		qR = qL = valC;
	}


	float flux = 0.0f;
	if (C > 0.0f)
	{
		//a + bx + cx^2 (C being the variable (x))
		const float qq = qR + C * (-qL - 2 * qR + 3 * valC) + C * C * (qL + qR - 2 * valC);
		flux = veli * qq;
	}
	else
	{
		//a + bx + cx^2 (C being the variable (x))
		const float qq = qR + C * (-2 * qL - qR + 3 * valC) + C * C * (qL + qR - 2 * valC);
		flux = veli * qq;
	}

	return flux;
}

__global__ void advectPPMX(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neigh,
	const boundsEnv bounds,
	const float dt)
{
	// One shared for values
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();

		fillSharedNeigh(sharedBlock, arrayIn, defaultVal, z, bounds);
		__syncthreads();

		// We do not return, we just continue and wait for the rest of the threads. 
		if (isGroundGPU(x, y, z)) continue;
		idx = getIdx(x, y, z);

		// Downwind is when velocity is coming from the back
		// And since we are currently looking forward, downwind is moving with use thus positive
		bool downWind = velfieldX[idx] >= 0.0f;

		float extraRight = 0.0f;
		// If not downwind, we need to use extra Up
		if (!downWind)
		{
			// Make sure to safely check if this index will be outside
			if (getSafeNeighbourAtOffset(neigh, idx, 2, RIGHT) != SKY)
			{
				fillDataBoundCon(bounds.sides, extraRight, sharedBlock[idxsData], defaultVal[y]);
			}
			else // Data is safe to retrieve
			{
				// Use shared data if possible for faster access
				extraRight = threadIdx.x >= 15 ? arrayIn[idx + 2] : sharedBlock[idxsData + 2];
			}
		}

		float valL = downWind ? sharedBlock[idxsData - 1] : sharedBlock[idxsData];
		float valR = downWind ? sharedBlock[idxsData + 1] : extraRight;
		float valC = downWind ? sharedBlock[idxsData] : sharedBlock[idxsData + 1];

		float fluxRight = advectPPMFlux(velfieldX[idx], valL, valC, valR, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velL = fillNeighbourData(neigh[idx].left, bounds, velfieldX, idx, -1, defaultVelX[y]);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velL < 0.0f;

		// If not downwind, we need to use extra Down
		float extraLeft = 0.0f;
		if (!downWind)
		{
			// Make sure to safely check if this index will be outside
			if (getSafeNeighbourAtOffset(neigh, idx, -2, LEFT) != SKY)
			{
				fillDataBoundCon(bounds.sides, extraLeft, sharedBlock[idxsData], defaultVal[y]);
			}
			else // Data is safe to retrieve
			{
				// Use shared data if possible for faster access
				extraLeft = threadIdx.x <= 0 ? arrayIn[idx - 2] : sharedBlock[idxsData - 2];
			}
		}


		valL = downWind ? sharedBlock[idxsData - 1] : extraLeft;
		valR = downWind ? sharedBlock[idxsData + 1] : sharedBlock[idxsData];
		valC = downWind ? sharedBlock[idxsData] : sharedBlock[idxsData - 1];

		float fluxLeft = advectPPMFlux(velL, valL, valC, valR, dt);

		arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxRight - fluxLeft);
	}
}

__global__ void advectPPMY(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neigh,
	const boundsEnv bounds,
	const float dt)
{
	// One shared for values
	__shared__ float sharedBlock[18 * 18];

	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();

		fillSharedNeigh(sharedBlock, arrayIn, defaultVal, z, bounds);
		__syncthreads();

		// We do not return, we just continue and wait for the rest of the threads. 
		if (isGroundGPU(x, y, z)) continue;
		idx = getIdx(x, y, z);

		// Downwind is when velocity is coming from the back
		// And since we are currently looking forward, downwind is moving with use thus positive
		bool downWind = velfieldY[idx] >= 0.0f;

		float extraUp = 0.0f;
		// If not downwind, we need to use extra Up
		if (!downWind) 
		{
			// Make sure to safely check if this index will be outside
			if (getSafeNeighbourAtOffset(neigh, idx, GRIDSIZESKYX * 2, UP) != SKY)
			{
				fillDataBoundCon(bounds.up, extraUp, sharedBlock[idxsData], defaultVal[y]);
			}
			else // Data is safe to retrieve
			{
				// Use shared data if possible for faster access
				extraUp = threadIdx.y >= 15 ? arrayIn[idx + GRIDSIZESKYX * 2] : sharedBlock[idxsData + sharedBlockWidth * 2];
			}
		}

		float valD = downWind ? sharedBlock[idxsData - sharedBlockWidth] : sharedBlock[idxsData];
		float valU = downWind ? sharedBlock[idxsData + sharedBlockWidth] : extraUp;
		float valC = downWind ? sharedBlock[idxsData] : sharedBlock[idxsData + sharedBlockWidth];

		float fluxUp = advectPPMFlux(velfieldY[idx], valD, valC, valU, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velD = fillNeighbourData(neigh[idx].down, bounds, velfieldY, idx, -GRIDSIZESKYX, 0.0f);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velD < 0.0f;

		// If not downwind, we need to use extra Down
		float extraDown = 0.0f;
		if (!downWind)
		{
			// Make sure to safely check if this index will be outside
			if (getSafeNeighbourAtOffset(neigh, idx, -GRIDSIZESKYX * 2, DOWN) != SKY)
			{
				fillDataBoundCon(bounds.ground, extraDown, sharedBlock[idxsData], defaultVal[y]);
			}
			else // Data is safe to retrieve
			{
				// Use shared data if possible for faster access
				extraDown = threadIdx.y <= 0 ? arrayIn[idx - GRIDSIZESKYX * 2] : sharedBlock[idxsData - sharedBlockWidth * 2];
			}
		}

		valD = downWind ? sharedBlock[idxsData - sharedBlockWidth] : extraDown;
		valU = downWind ? sharedBlock[idxsData + sharedBlockWidth] : sharedBlock[idxsData];
		valC = downWind ? sharedBlock[idxsData] : sharedBlock[idxsData - sharedBlockWidth];

		float fluxDown = advectPPMFlux(velD, valD, valC, valU, dt);

		arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxUp - fluxDown);
	}

}

__global__ void advectPPMZ(const float* __restrict__ arrayIn, 
	float* __restrict__ arrayOut, 
	const float* __restrict__ defaultVal, 
	const float* __restrict__ velfieldZ, 
	const Neigh* __restrict__ neigh, 
	const boundsEnv bounds, 
	const float dt)
{
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	// Make sure to not access nullptr
	float defaultValue = defaultVal ? defaultVal[y] : 0.0f;

	// Already set forward and current to the position where they can get easily swapped in the for loop
	float forwardExtra = fillNeighbourData(neigh[idx].forward, bounds, arrayIn, idx, GRIDSIZESKYX * GRIDSIZESKYY, defaultValue);
	float forward = arrayIn[idx];
	float current = fillNeighbourData(neigh[idx].backward, bounds, arrayIn, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultValue);
	float backward = fillNeighbourData(getSafeNeighbourAtOffset(neigh, idx, -GRIDSIZESKYX * GRIDSIZESKYY * 2, BACKWARD), bounds, arrayIn, idx, -GRIDSIZESKYX * GRIDSIZESKYY * 2, defaultValue);
	float backwardExtra = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// We do not return, we just continue 
		if (isGroundGPU(x, y, z)) continue;
		idx = getIdx(x, y, z);

		backwardExtra = backward;
		backward = current;
		current = forward;
		forward = forwardExtra;
		forwardExtra = fillNeighbourData(getSafeNeighbourAtOffset(neigh, idx, GRIDSIZESKYX * GRIDSIZESKYY * 2, FORWARD), bounds, arrayIn, idx, GRIDSIZESKYX * GRIDSIZESKYY * 2, defaultValue);


		// Downwind is when velocity is coming from the back
		// And since we are currently looking forward, downwind is moving with use thus positive
		bool downWind = velfieldZ[idx] >= 0.0f;

		float valB = downWind ? backward : current;
		float valF = downWind ? forward : forwardExtra;
		float valC = downWind ? current : forward;

		float fluxForward = advectPPMFlux(velfieldZ[idx], valB, valC, valF, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velB = fillNeighbourData(neigh[idx].backward, bounds, velfieldZ, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultVelZ[y]);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velB < 0.0f;

		valB = downWind ? backward : backwardExtra;
		valF = downWind ? forward : current;
		valC = downWind ? current : backward;

		float fluxBackward = advectPPMFlux(velB, valB, valC, valF, dt);


		arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxForward - fluxBackward);
		//if (x == 0 && y == 16) printf("x %i y: %i, z %i value: %f, value2 %f, final %f, FE %f, F %f, C %f, B %f, BE %f, valuecurrent: %f\n", x, y, z, fluxForward, fluxBackward, (dt / VOXELSIZE) * (fluxForward - fluxBackward), forwardExtra, forward, current, backward, backwardExtra, test);
		//if (x == 0 && y == 16) printf("x %i y: %i, z %i value: %f, value2 %f, final %f, prev: %f, now: %f, vel %f, velB %f\n", x, y, z, fluxForward, fluxBackward, (dt / VOXELSIZE) * (fluxForward - fluxBackward), arrayIn[idx], arrayOut[idx], velfieldZ[idx], velB);
	}
}

__global__ void advectPrecipGPU(float* Qj, const Neigh* neigh, const float* potTemp, const float* Qv,
	const float* pressuresAir, const float* groundP,const int type, const float dt)
{
	// Shared block as big as the whole block length
	__shared__ float sharedBlock[GRIDSIZESKYY];
	// We swap x and y around, meaning:
	// 1 block is all the y values
	// then the block index is the x value
	int x = blockIdx.x;
	int y = threadIdx.x;
	int z = 0;
	int idx = getIdx(x, y, z);

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();

		idx = getIdx(x, y, z);

		// Fill shared data
		sharedBlock[y] = Qj[idx];
		__syncthreads();

		// We do not return, since we still need to fill data
		if (isGroundGPU(x, y, z)) continue;

		const int idxG = x + z * GRIDSIZESKYX;
		const int idxU = idx + GRIDSIZESKYX;
		const bool up = neigh[idx].up == SKY;

		const float QjC = sharedBlock[y];
		const float QjU = up ? sharedBlock[y + 1] : 0.0f;

		float fallVel = 0.0f;
		float fallVelUp = 0.0f;

		float3 fallVelPrecip{ 0.0f };
		float3 fallVelPrecipUp{ 0.0f };

		// if, nah Im kidding
		{
			const float T = float(potTemp[idx]) * powf(pressuresAir[idx] / groundP[idxG], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			const float Tv = T * (0.608f * Qv[idx] + 1);
			const float density = pressuresAir[idx] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa

			fallVelPrecip = calculateFallingVelocityGPU(QjC, QjC, QjC, density, type, gammaR, gammaS, gammaI);
		}
		if (neigh[idx].up == SKY)
		{
			const float T = float(potTemp[idxU]) * powf(pressuresAir[idxU] / groundP[idxG], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			const float Tv = T * (0.608f * Qv[idxU] + 1);
			const float density = pressuresAir[idxU] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa

			fallVelPrecipUp = calculateFallingVelocityGPU(QjU, QjU, QjU, density, type, gammaR, gammaS, gammaI);
		}

		switch (type)
		{
		case 0:
			fallVel = fallVelPrecip.x;
			fallVelUp = fallVelPrecipUp.x;
			break;
		case 1:
			fallVel = fallVelPrecip.y;
			fallVelUp = fallVelPrecipUp.y;
			break;
		case 2:
			fallVel = fallVelPrecip.z;
			fallVelUp = fallVelPrecipUp.z;
			break;
		default:
			break;
		}

		Qj[idx] -= dt * fmin((fallVel / VOXELSIZE) * QjC, QjC); //Remove % of precip
		if (neigh[idx].up == SKY)
		{
			Qj[idx] += dt * fmin((fallVelUp / VOXELSIZE) * QjU, QjU); //Grab % of precip above
		}
	}
}

__global__ void applyPreconditionerGPU(float* output, const float* precon, const float* div, char4* A)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		const int idx = getIdx(x, y, z);
		output[idx] = 0.0f;
		if (isGroundGPU(x, y, z)) continue;

		output[idx] = div[idx] * precon[idx];
	}
}

__global__ void dotProductGPU(float* result, const float* a, const float* b)
{
	//To speed up dot product, we make all blocks sum up their values and then connect them together.
	//This is faster than 1 thead doing all the work.
	__shared__ float sresult[GRIDSIZESKYX];
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	const int idx = getIdx(x, y, 0);

	sresult[x] = 0.0f;
	if (!isGroundGPU(x, y, 0))
	{
		sresult[x] = a[idx] * b[idx];
	}
	__syncthreads();

	// We now add multiply and add the values on the z direction, this would be the slowest part
	for (int z = 1; z < GRIDSIZESKYZ; z++)
	{
		if (isGroundGPU(x, y, z)) continue;
		const int idx = getIdx(x, y, z);

		sresult[x] += a[idx] * b[idx];
	}
	__syncthreads();


	//Basically, we grab half of the block, add all the values on the other side and repeat the process.
	for (int i = GRIDSIZESKYX / 2; i > 0; i >>= 1)
	{
		if (x < i)
		{
			sresult[x] += sresult[x + i];
		}
		__syncthreads();
	}

	//Using atomicAdd(), we can safely add all block values to a singular value
	if (x == 0)
	{
		atomicAdd(result, sresult[0]);
	}
}

__global__ void applyAGPU(float* output, const float* input, const Neigh* neigh, const char4* A, boundsEnv bounds)
{
	//Using shared data and forward + backward due to input need from all direction
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	float forward = input[idx];
	float current = fillNeighbourData(neigh[idx].backward, bounds, input, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();
		idx = getIdx(x, y, z);

		fillSharedNeigh(sharedBlock, input, nullptr, z, bounds);

		backward = current;
		current = forward;
		forward = fillNeighbourData(neigh[idx].forward, bounds, input, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);

		__syncthreads();

		output[idx] = 0.0f;
		// We do not return, since we still need to fill data
		if (isGroundGPU(x, y, z)) continue;

		float l = sharedBlock[idxsData - 1];
		float r = sharedBlock[idxsData + 1];
		float d = sharedBlock[idxsData - sharedBlockWidth];
		float u = sharedBlock[idxsData + sharedBlockWidth];
		float f = forward; //Useless, but to keep consistancy with naming
		float b = backward;

		// TODO: left and backwards 0 when not sky?
		char ALeft = (neigh[idx].left == SKY) ? A[idx - 1].x : 0;
		char ADown = (neigh[idx].down == SKY) ? A[idx - GRIDSIZESKYX].y : 0;
		char ABack = (neigh[idx].backward == SKY) ? A[idx - GRIDSIZESKYX * GRIDSIZESKYY].z : 0;
		char4 ACur = A[idx];

		output[idx] = ACur.z * input[idx] +
			((ALeft * l +
				ADown * d +
				ABack * b +
				ACur.x * r +
				ACur.y * u +
				ACur.z * f)
				);
	}
}

__global__ void calculateDivergenceGPU(float* divergence, const Neigh* neigh, const float* velX, const float* velY, const float* velZ, 
	const float* dens, const float* oldDens, boundsEnv bounds, boundsEnv boundsVel, const float dt)
{
	//We are not using shared data due to different input and default values
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		const int idx = getIdx(x, y, z);
		float current = dens[idx];

		// Velocities
		float ul = 0.0f, ur = 0.0f, ud = 0.0f, uu = 0.0f, uf = 0.0f, ub = 0.0f;
		// Densities
		float dl = 0.0f, dr = 0.0f, dd = 0.0f, du = 0.0f, df = 0.0f, db = 0.0f;

		divergence[idx] = 0.0f;

		// We do not return, since we still need to fill data
		if (isGroundGPU(x, y, z)) continue;

		ur = fillNeighbourData(neigh[idx].right, boundsVel, velX, idx, 1, defaultVelX[y]);
		ul = fillNeighbourData(neigh[idx].left, boundsVel, velX, idx, -1, defaultVelX[y]);
		uu = fillNeighbourData(neigh[idx].up, boundsVel, velY, idx, GRIDSIZESKYX, 0.0f, true);
		ud = fillNeighbourData(neigh[idx].down, boundsVel, velY, idx, -GRIDSIZESKYX, 0.0f);
		uf = fillNeighbourData(neigh[idx].forward, boundsVel, velZ, idx, GRIDSIZESKYX * GRIDSIZESKYY, defaultVelZ[y]);
		ub = fillNeighbourData(neigh[idx].backward, boundsVel, velZ, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultVelZ[y]);

		// Grab the harmonic mean of the densities and normalize
		dr = dr == 0.0f ? 0.0f : 2.0f / (1.0f / current + 1.0f / dr);
		dl = dl == 0.0f ? 0.0f : 2.0f / (1.0f / current + 1.0f / dl);
		du = du == 0.0f ? 0.0f : 2.0f / (1.0f / current + 1.0f / du);
		dd = dd == 0.0f ? 0.0f : 2.0f / (1.0f / current + 1.0f / dd);
		df = df == 0.0f ? 0.0f : 2.0f / (1.0f / current + 1.0f / df);
		db = db == 0.0f ? 0.0f : 2.0f / (1.0f / current + 1.0f / db);

		const float massFluxDiv = ((dr * ur - dl * ul) + (du * uu - dd * ud) + (df * uf - db * ub));

		//Change in density per second (which is why we use dt)
		const float densityChange = (current - oldDens[idx]) / dt;

		//Using divergence minus the change in compressibility
		//Dividing again by dt to get kg/m3*s2 instead of only s TODO: should we?
		divergence[idx] = (massFluxDiv - densityChange);
	}
}

__global__ void applyPresProjGPU(const float* pressure, const Neigh* neigh, float* velX, float* velY, float* velZ, const float* density,const float* pressureEnv, 
	boundsEnv boundsX, boundsEnv boundsY, boundsEnv boundsZ, boundsEnv boundsDens, const float dt)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;


	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		if (isGroundGPU(x, y, z)) continue;
		const int idx = getIdx(x, y, z);

		// Pressures
		float pr = 0.0f, pu = 0.0f, pf = 0.0f;
		float currentP = pressure[idx];
		// Densities
		float dr = 0.0f, du = 0.0f, df = 0.0f;
		float currentD = density[idx];

		//If at the right or upper cell, we don't add any value
		pr = fillNeighbourData(neigh[idx].right, boundsX, pressure, idx, 1, 0.0f);
		pu = fillNeighbourData(neigh[idx].up, boundsY, pressure, idx, GRIDSIZESKYX, 0.0f);
		pf = fillNeighbourData(neigh[idx].forward, boundsZ, pressure, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
		pr -= currentP;
		pu -= currentP;
		pf -= currentP;

		//Pressure at face
		dr = fillNeighbourData(neigh[idx].right, boundsDens, density, idx, 1, 0.0f);
		du = fillNeighbourData(neigh[idx].up, boundsDens, density, idx, GRIDSIZESKYX, 0.0f);
		df = fillNeighbourData(neigh[idx].forward, boundsDens, density, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
		// Calculate harmonic mean
		dr = 2.0f / (1.0f / currentD + 1.0f / currentP);
		du = 2.0f / (1.0f / currentD + 1.0f / currentP);
		df = 2.0f / (1.0f / currentD + 1.0f / currentP);

		//Using dt to match dt used in divergence calculation
		//Dividing by density at cell faces gives for pressure effects
		velX[idx] += pr / dr;
		velY[idx] += pu / du;
		velZ[idx] += pf / df;
	}
}

__global__ void getMaxDivergence(float* output, const float* div)
{
	//To speed up getting the max divergence, we make all blocks sum up their values and then connect them together.
	//This is faster than 1 thead doing all the work.
	__shared__ float sresult[GRIDSIZESKYX];
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	const int idx = getIdx(x, y, 0);

	float maxVal = 0.0f;
	if (!isGroundGPU(x, y, 0))
	{
		maxVal = fabsf(div[idx]);
	}
	sresult[x] = maxVal;
	__syncthreads();

	// We now check all the values on the z direction, this would be the slowest part
	for (int z = 1; z < GRIDSIZESKYZ; z++)
	{
		if (isGroundGPU(x, y, z)) continue;
		sresult[x] = fmaxf(sresult[x], div[getIdx(x, y, z)]);
	}
	__syncthreads();


	// Basically, we grab half of the block, add all the values on the other side and repeat the process.
	for (int i = GRIDSIZESKYX / 2; i > 0; i >>= 1)
	{
		if (x < i)
		{
			sresult[x] = fmaxf(sresult[x] ,sresult[x + i]);
		}
		__syncthreads();
	}

	// Using atomicAdd(), we can safely add all block values to a singular value
	if (x == 0)
	{
		//Using atomic max, supports only ints, so we cast sort of to float
		atomicMax((int*)output, __float_as_int(sresult[0]));
		//This should work if we don't have negative numbers (which we should not have)
	}
}

__global__ void updatePandDiv(float* S1, float* S2, float* pressure, float* divergence, const float* s, const float* valZ)
{
	__shared__ float sresult;
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;

	if (x == 0)
	{
		sresult = *S1 / *S2;
	}

	__syncthreads();
	
	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		//Adding up pressure value and reducing residual value
		if (!isGroundGPU(x, y, z))
		{
			const int idx = getIdx(x, y, z);
			pressure[idx] += sresult * s[idx];
			divergence[idx] -= sresult * valZ[idx];
		}
	}
	if (x == 0 && y == 0)
	{
		*S2 = sresult;
	}
}

__global__ void endIteration(float* S1, float* S2, float* s, const float* valZ)
{
	__shared__ float sresult;
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;

	if (x == 0)
	{
		sresult = *S2 / *S1;
	}
	__syncthreads();

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		if (!isGroundGPU(x, y, z))
		{
			const int idx = getIdx(x, y, z);
			//Setting search vector s
			s[idx] = valZ[idx] + sresult * s[idx];
		}
	}

	if (x == 0 && y == 0)
	{
		*S1 = *S2;
	}
}

__global__ void updatePressure(float* envPressure, const float* presProj)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;
	
	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		const int idx = getIdx(x, y, z);
		envPressure[idx] -= presProj[idx] / 100.0f; //Pa to Pha
	}
}

//-------------------------------------OTHER-------------------------------------

__global__ void calculateCloudCoverGPU(float* output, const float* Qc, const float* Qw, const int* GHeight)
{
	const int x = threadIdx.x;
	const int z = threadIdx.x;
	const int idx = x + z * GRIDSIZESKYX;

	float totalCloudContent = 0.0f;
	const float qfull = 1.2f; // a threshold value where all incoming radiation is reflected by cloud matter: http://meto.umd.edu/~zli/PDF_papers/Li%20Nature%20Article.pdf
	for (int y = GHeight[idx] + 1; y < GRIDSIZESKYY; y++) totalCloudContent += (Qc[getIdx(x, y, z)] + Qw[getIdx(x, y, z)]) * VOXELSIZE;
	output[idx] = fmin(totalCloudContent / qfull, 1.0f);
}

__global__ void calculateGroundTempGPU(float* groundT, const float dtSpeed, const float irridiance, const float* LC)
{
	const int x = threadIdx.x;
	const int z = threadIdx.x;
	const int idx = x + z * GRIDSIZESKYX;

	const float groundTemp = groundT[idx];
	const float absorbedRadiationAlbedo = 0.25f;  //How much light is reflected back? 0 = absorbes all, 1 = reflects all
	const float groundThickness = 1.0f; //Just used 1 meter
	const float densityGround = 1500.0f;
	const double T4 = groundTemp * groundTemp * groundTemp * groundTemp;

	groundT[idx] += dtSpeed * ((1 - LC[idx]) * (((1 - absorbedRadiationAlbedo) * irridiance - ConstantsGPU::ge * ConstantsGPU::oo * T4) / (groundThickness * densityGround * ConstantsGPU::Cpds)));
}

__global__ void buoyancyGPU(float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* pressures, const float* groundP, float* buoyancyStor, const float , boundsEnv bounds, const float dt)
{
	__shared__ float sharedBlockQv[18 * 18];
	__shared__ float sharedBlockT[18 * 18];

	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	bool up = neigh[idx].up == SKY;
	bool down = neigh[idx].down == SKY;

	float forwardQv = Qv[idx];
	float forwardT = potTemp[idx];
	float currentQv = 0.0f;
	float currentT = 0.0f;
	currentQv = fillNeighbourData(neigh[idx].backward, bounds, Qv, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
	currentT = fillNeighbourData(neigh[idx].backward, bounds, potTemp, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
	float backwardQv = 0;
	float backwardT = 0;


	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();
		idx = getIdx(x, y, z);

		backwardQv = currentQv;
		backwardT = currentT;
		currentQv = forwardQv;
		currentT = forwardT;
		forwardQv = fillNeighbourData(neigh[idx].forward, bounds, Qv, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
		forwardT = fillNeighbourData(neigh[idx].forward, bounds, potTemp, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
		const float groundPs = groundP[x + z + GRIDSIZESKYX];
		const float	psC = pressures[idx];
		const float psU = pressures[idx + GRIDSIZESKYX];
		const float psD = pressures[idx - GRIDSIZESKYX];

		fillSharedNeigh(sharedBlockQv, Qv, nullptr, z, bounds);
		fillSharedNeigh(sharedBlockT, potTemp, nullptr, z, bounds);
		__syncthreads();

		// We do not return, we just continue 
		if (isGroundGPU(x, y, z)) continue;


		// ------------ QV ------------

		//Vapor environment and Vapor Parcel
		float Qenv = 0.0f, QenvUp = 0.0f, QenvDown = 0.0f;
		float QP = 0.0f;

		QP = sharedBlockQv[idxsData];
		Qenv = (QP + sharedBlockQv[idxsData + 1] + sharedBlockQv[idxsData - 1] + forwardQv + backwardQv) / 5.0f;
		QenvUp = sharedBlockQv[idxsData + sharedBlockWidth];
		QenvDown = sharedBlockQv[idxsData - sharedBlockWidth];


		// ------------ Temp ------------

		const float TDown = sharedBlockQv[idxsData - sharedBlockWidth] * powf(psD / groundPs, ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float T = sharedBlockQv[idxsData] * powf(psC / groundPs, ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float TUp = sharedBlockQv[idxsData + sharedBlockWidth] * powf(psU / groundPs, ConstantsGPU::Rsd / ConstantsGPU::Cpd);

		// Temp for up and down based on adiabatics
		float Tadiab = sharedBlockT[idxsData], TadiabUp = sharedBlockT[idxsData], TadiabDown = sharedBlockT[idxsData];
		// Environment Temp
		float Tenv = 0.0f, TenvUp = 0.0f, TenvDown = 0.0f;
		Tenv = (Tadiab + sharedBlockT[idxsData + 1] + sharedBlockT[idxsData - 1] + forwardT + backwardT) / 5.0f;
		TenvUp = sharedBlockT[idxsData + sharedBlockWidth];
		TenvDown = sharedBlockT[idxsData - sharedBlockWidth];


		if (down)
		{
			//If going downwards, we still check if this downwards parcel would be saturated
			if ((T < 0.0f && QP >= wiGPU((TDown - 273.15f), psD)) || QP >= wsGPU((TDown - 273.15f), psD))
			{
				//Need real temperature to calculate moist adiabatic
				TadiabDown = T - MLRGPU(T - 273.15f, psC) * (psC - psD);
				//Convert back to potTemp
				TadiabDown = TadiabDown * powf(groundPs / psD, ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			}
		}
		if (up)
		{
			//Vapor amount would be greater than downwards temp could hold, so we use moist adiabatic
			if ((T < 0.0f && QP >= wiGPU((TUp - 273.15f), psU)) || QP >= wsGPU((TUp - 273.15f), psU))
			{
				//Need real temperature to calculate moist adiabatic
				TadiabUp = MLRGPU(T - 273.15f, psC) * (psU - psC) + T;
				//Convert back to potTemp
				TadiabUp = TadiabUp * powf(groundPs / psU, ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			}
		}


		// ------------ Buoyancy ------------


		// Seperate buoyancies
		float B = 0.0f, BUp = 0.0f, BDown = 0.0f;

		const float totalQ = 0.0f;// Qr[idx] + Qs[idx] + Qi[idx];
		if (down)
		{
			//Temp parcel and Temp environment
			const float VTP = TadiabDown * (1.0f + 0.608f * QP);
			const float VTE = TenvDown * (1.0f + 0.608f * QenvDown);
			BDown = ConstantsGPU::g * ((VTP - VTE) / VTE) - totalQ * ConstantsGPU::g;
		}
		{
			//Temp parcel and Temp environment
			const float VTP = Tadiab * (1.0f + 0.608f * QP);
			const float VTE = Tenv * (1.0f + 0.608f * Qenv);
			B = ConstantsGPU::g * ((VTP - VTE) / VTE) - totalQ * ConstantsGPU::g;
		}
		if (up)
		{
			//Temp parcel and Temp environment
			const float VTP = TadiabUp * (1.0f + 0.608f * QP);
			const float VTE = TenvUp * (1.0f + 0.608f * QenvUp);
			BUp = ConstantsGPU::g * ((VTP - VTE) / VTE) - totalQ * ConstantsGPU::g;
		}


		// Now we calculate if we use up or down, this depends if the parcel on the current levels is going up or down
		float buoyancyFinal = B;

		//Using the velocity, we track if the parcel is already going up or down, suggesting which layer we are about to enter.
		if (velY[idx] > 0.0f && up) {
			buoyancyFinal = BUp; //Going up, thus use the buoyancy from above, if it is much warmer, this will be negative, meaning descending of parcel
		}
		else if (velY[idx] < 0.0f && down) {
			buoyancyFinal = BDown; //Going up, thus buoyancy from below, if it is much colder, this will be positive, meaning rising of parcel again
		}

		// With the correct buoyancy applied we could just insert it into the velocity, yet this causes a lot of back and forwarding.
		// Instead, we limit this back and forwarding
		// When we change the velocity due to buoyancy too much (going from positive to negative of visa versa), we just set velocity to 0.
		// This makes the air much more stable

		float change = buoyancyFinal * dt;
		if ((velY[idx] > 0.0f && -change > velY[idx]) || (velY[idx] < 0.0f && -change < velY[idx]))
		{
			change = -velY[idx];
		}

		buoyancyStor[idx] = change;
		velY[idx] += change;
	}
}

__global__ void addHeatGPU(const float* _Qv, float* potTemp, float* condens, float* depos, float* freeze)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int z = 0; 

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdx(x, y, z);

		const float Qv = _Qv[idx];
		const float Mair = 0.02896f; //In kg/mol
		const float Mwater = 0.01802f; //In kg/mol
		const float XV = (Qv / Mwater) / ((Qv / Mwater) + (1 - Qv) / Mair);
		const float Mth = XV * Mwater + (1 - XV) * Mair;
		const float Yair = 1.4f, yV = 1.33f;
		const float YV = XV * (Mwater / Mth); //Mass fraction of vapor
		const float yth = YV * yV + (1 - YV) * Yair; //Weighted average

		const float cpth = yth * ConstantsGPU::R / (Mth * (yth - 1)); // Get specific gas constant

		float sumPhaseheat = 0.0f;

		sumPhaseheat += LwaterGPU(potTemp[idx] - 273.15f) / cpth * condens[idx];
		sumPhaseheat += LiceGPU(potTemp[idx] - 273.15f) / cpth * depos[idx];
		sumPhaseheat += ConstantsGPU::Lf / cpth * freeze[idx];

		condens[idx] = 0.0f;
		freeze[idx] = 0.0f;
		depos[idx] = 0.0f;

		potTemp[idx] += sumPhaseheat;
	}
}

__global__ void computeNeighbourGPU(Neigh* Neigh)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdx(x, y, z);

		Neigh[idx].left =  (x == 0) ? OUTSIDE : (isGroundGPU(x - 1, y, z) ? GROUND : SKY);
		Neigh[idx].right = (x == GRIDSIZESKYX - 1) ? OUTSIDE : (isGroundGPU(x + 1, y, z) ? GROUND : SKY);
		Neigh[idx].down =  (y == 0) ? GROUND : (isGroundGPU(x, y - 1, z) ? GROUND : SKY); // Down is always ground (at y == 0)
		Neigh[idx].up =    (y == GRIDSIZESKYY - 1) ? OUTSIDE : (isGroundGPU(x, y + 1, z) ? GROUND : SKY);
		Neigh[idx].backward = (z == 0) ? OUTSIDE : (isGroundGPU(x, y, z - 1) ? GROUND : SKY);
		Neigh[idx].forward = (z == GRIDSIZESKYZ - 1) ? OUTSIDE : (isGroundGPU(x, y, z + 1) ? GROUND : SKY);
	}
}



__global__ void initAMatrix(char4* A, const Neigh* neigh, const float* density, boundsEnv bounds)
{
	printf("tst\n");
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	// Already set forward and current to the position where they can get easily swapped in the for loop
	float forward = density[idx];
	float current = 0.0f;
	current = fillNeighbourData(neigh[idx].backward, bounds, density, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();
		idx = getIdx(x, y, z);

		backward = current;
		current = forward;
		forward = fillNeighbourData(neigh[idx].forward, bounds, density, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);

		fillSharedNeigh(sharedBlock, density, nullptr, z, bounds);
		__syncthreads();

		float l = 0.0f, r = 0.0f, d = 0.0f, u = 0.0f, f = 0.0f, b = 0.0f;
		const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

		// We do not return, since we still need to fill data
		if (isGroundGPU(x, y, z)) continue;


		//Fill matrix A	
		A[idx].x = 0;
		A[idx].y = 0;
		A[idx].z = 0;
		A[idx].w = 0;

		r = sharedBlock[idxsData + 1];
		l = sharedBlock[idxsData - 1];
		u = sharedBlock[idxsData + sharedBlockWidth];
		d = sharedBlock[idxsData - sharedBlockWidth];
		f = forward;
		b = backward;

		//Calculate harmonic mean: https://en.wikipedia.org/wiki/Harmonic_mean
		//Also normalizing (1.225 / answer) to make sure values don't reach below 1.0f (if so, it would crash)
		//Using max for if density > 1.225, and the -  is to make values negative.
		r = r == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / current + 1.0f / r)));
		l = l == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / current + 1.0f / l)));
		u = u == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / current + 1.0f / u)));
		d = d == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / current + 1.0f / d)));
		f = f == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / current + 1.0f / f)));
		b = b == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / current + 1.0f / b)));
		
		//Set positive directions for A matrix
		A[idx].x = r;
		A[idx].y = u;
		A[idx].z = f;
		//Using - because calculated density is already set to be negative, so this makes positive
		A[idx].w -= r;
		A[idx].w -= l;
		A[idx].w -= u;
		A[idx].w -= d;
		A[idx].w -= f;
		A[idx].w -= b;
	}
}

__global__ void initPrecon(float* precon, const char4* A)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdx(x, y, z);

		precon[idx] = 1.0f;

		if (isGroundGPU(x, y, z)) continue;

		precon[idx] = A[idx].w == 0.0f ? 1.0f : 1.0f / A[idx].w;
	}

	//const float Tune = 0.97f;
	//
	////TODO: for 3D, look at formula 4.34	
	//int Aminxx = tX == 0 ? 0 : -1;//A[idx - 1].x; //Minus X looking at x
	//int Aminxy = tX == 0 ? 0 : -1;//A[idx - 1].y; //Minus X looking at y
	//int Aminyy = tY == 0 ? 0 : -1;//A[idx - GRIDSIZESKYX].y;	//Minus Y looking at y
	//int Aminyx = tY == 0 ? 0 : -1;//A[idx - GRIDSIZESKYX].x;	//Minus Y looking at x

	////This does not have effect due to precon calculated in parrallel. 
	//const float Preconi = 0.0f;//tX == 0 ? precon[idx] : precon[idx - 1];
	//const float Preconj = 0.0f;//tY == 0 ? precon[idx] : precon[idx - GRIDSIZESKYX];
	//const float e = A[idx].z
	//	- (Aminxx * Preconi) * (Aminxx * Preconi)
	//	- (Aminyy * Preconj) * (Aminyy * Preconj)
	//	- Tune * (
	//		Aminxx * Aminxy * (Preconi * Preconi) +
	//		Aminyy * Aminyx * (Preconj * Preconj));
	//precon[idx] = (1 / sqrtf(e + 1e-30f)); //Prevent division by 0 using small number;
}

__global__ void initDensity(float* densityAir, const float* potTemp, const float* pressures, const float* Qv, const float* groundP)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int z = 0;
	
	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdx(x, y, z);
		const int idxG = x + z * GRIDSIZESKYX;

		const float T = float(potTemp[idx]) * glm::pow(pressures[idx] / groundP[idxG], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressures[idx] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa

		densityAir[idx] = density;
	}
}

__global__ void calculateNewPressure(float* pressureEnv, const float* densityAir, const float* potTemp, const float* Qv, const float* GPressure)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		const int idx = getIdx(x, y, z);
		const int idxG = x + z * GRIDSIZESKYX;

		//Yes, we use pressure to first calculate T and then calculate pressure, it is double sided but how else?
		const float T = float(potTemp[idx]) * glm::pow(pressureEnv[idx] / GPressure[idxG], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		pressureEnv[idx] = densityAir[idx] * ConstantsGPU::Rsd * Tv / 100.0f;
	}

}

__global__ void applyBrushGPU(float* array, float* array2, int* groundGridStor, bool* changedGround, parameter paramType, const float brushSize, const float2 mousePos, 
	const float2 extras, const float brushSmoothness, const float brushIntensity, const float applyValue, const float2 valueDir, const bool groundErase, const Neigh* neigh, const float dt)
{
	//Thread
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;

	const int brushOffset = int(ceilf(brushSize));
	//Local X and Y (is offset half to the bottom left)
	const int lX = tX - brushOffset;
	const int lY = tY - brushOffset;

	//Including mouse and correct brush offset
	const int mX = tX + int(roundf(mousePos.x)) - brushOffset;
	const int mY = tY + int(roundf(mousePos.y)) - brushOffset;
	const int mIdx = mX + mY * GRIDSIZESKYX;

	if (mX >= GRIDSIZESKYX || mY >= GRIDSIZESKYY || mX < 0 || mY < 0) return;

	//Can't brush the ground
	if (paramType != PGROUND && mY <= GHeight[mX]) return;

	//Distance check
	const float eX = lX + extras.x;
	const float eY = lY + extras.y;
	const float distance = eX * eX + eY * eY; //This works since we determine from 0.
	const float radius = brushSize * brushSize;
	if (distance <= radius) //Within range
	{
		//Based on distance, smoothness and intensity, add value.
		//Distance
		float value1 = -(distance / radius - 1);

		//Smoothness (For now just a simple squared)
		value1 = glm::pow(value1, brushSmoothness);

		//Intensity
		value1 *= dt * brushIntensity * applyValue;

		//For directional value
		float value2 = 0.0f;
		if (paramType == 7)
		{
			value2 = value1 * valueDir.y;
			value1 *= valueDir.x;
		}

		//Apply
		if (mX >= 0 && mX < GRIDSIZESKYX && mY >= 0 && mY < GRIDSIZESKYY)
		{
			if (paramType == PGROUND) //For ground
			{
				const bool groundChanged = setGround(groundGridStor, mX, mY, groundErase);
				if (groundChanged) *changedGround = groundChanged; //We don;t want to set back to false, if ever set, it is that.
			}
			else if (paramType == WIND) //For wind
			{
				array[mIdx] += value1;
				array2[mIdx] += value2;
			}
			else
			{
				array[mIdx] += value1;
			}
		}
	}
}

__device__ bool setGround(int* groundHeight, const int x, const int y, const bool eraseGround)
{

	if (eraseGround && y <= GHeight[x])
	{
		int oldVal = atomicMin(&groundHeight[x], y);
		return true;
	}
	else if (!eraseGround && y > GHeight[x])
	{
		int oldVal = atomicMax(&groundHeight[x], y);
		return true;
	}
	return false;
}

__global__ void compareAndResetValuesOutGround(const int* oldGroundHeight, const int* newGroundHeight, const float* isentropicTemp, const float* isentropicVap, 
	float* Qv, float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* potTemp, float* velX, float* velY, float* pres, float* defaultPres)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	//return if Y is not at in between the old and new ground height

	if (tY > oldGroundHeight[tX] || tY <= newGroundHeight[tX]) return;
	Qv[idx] = isentropicVap[tY];
	Qw[idx] = 0.0f;
	Qc[idx] = 0.0f;
	Qr[idx] = 0.0f;
	Qs[idx] = 0.0f;
	Qi[idx] = 0.0f;
	potTemp[idx] = isentropicTemp[tY];
	velX[idx] = 0.0f;
	velY[idx] = 0.0f;
	pres[idx] = defaultPres[tY];
}


//-------------------------------------HELPER-------------------------------------


__global__ void resetVelPressProj(const Neigh* neigh, float* velX, float* velY, float* velZ)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int z = 0;
	int idx = getIdx(x, y, z);

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		if (isGroundGPU(x, y, z)) return;
		idx = getIdx(x, y, z);

		if (neigh[idx].up != SKY)
		{
			velY[idx] = 0.0f;
		}
		if (neigh[idx].right != SKY)
		{
			velX[idx] = defaultVelX[y];
		}
		if (neigh[idx].forward != SKY)
		{
			velZ[idx] = defaultVelZ[y];
		}
	}
}

__device__ bool isGroundLevel(const int z)
{
	if (z >= GRIDSIZESKYZ || z < 0)
	{
		printf("Warning, trying to access z values outside of the grid in isGroundGPU\n");
		return true;
	}
	int x = threadIdx.x;
	int y = blockIdx.x;
	return y == (GHeight[x + z * GRIDSIZESKYZ] + 1);
}

//Usage of threads
__device__ bool isGroundGPU(const int z)
{
	if (z >= GRIDSIZESKYZ || z < 0)
	{
		printf("Warning, trying to access z values outside of the grid in isGroundGPU\n");
		return true;
	}
	int x = threadIdx.x;
	int y = blockIdx.x;
	return y <= GHeight[x + z * GRIDSIZESKYX];
}

__device__ bool isGroundGPU(const int x, const int y, const int z)
{
	return y <= GHeight[x + z * GRIDSIZESKYX];
}

__global__ void setToDefault(float* array, const float* defaultValue)
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	int z = 0;
	int idx = getIdx(x, y, z);

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		if (isGroundGPU(x, y, z)) return;
		idx = getIdx(x, y, z);

		array[idx] = defaultValue[y];
	}
}


__device__ void fillSharedNeigh(float* sharedData, const float* data, const float* customData, const int z, boundsEnv bounds)
{
	const int x = threadIdx.x + blockDim.x * blockIdx.x;
	const int y = threadIdx.y + blockDim.y * blockIdx.y;
	const int idx = getIdx(x, y, z);

	float customDataVal = 0.0f;
	if (customData)
	{
		customDataVal = customData[y];
	}
	float dataVal = data[idx];

	// We add offset since our halo is 18x18
	int sharedIdx = threadIdx.x + 1 + (threadIdx.y + 1) * (blockDim.x + 2);

	// Create offset if on the edge of this block
	const int extraIdxUp = threadIdx.y == blockDim.y - 1 ? 1 : 0;
	const int extraIdxRight = int(threadIdx.x == blockDim.x - 1);
	const int extraIdxDown = threadIdx.y == 0 ? -1 : 0;
	const int extraIdxLeft = -int((threadIdx.x == 0));

	envType horType = SKY;
	envType verType = SKY;

	// Set types on the horizontal and vertical axis, since we can be on a corner
	int extraX = x + extraIdxRight + extraIdxLeft;
	int extraY = y + extraIdxUp + extraIdxDown;
	if (extraX >= GRIDSIZESKYX || extraX < 0) horType = OUTSIDE;
	else if (isGroundGPU(extraX, y, z)) horType = GROUND;
	if (extraY >= GRIDSIZESKYY) verType = OUTSIDE;
	else if (extraY < 0 || isGroundGPU(x, extraY, z)) verType = GROUND;

	// We can already set our current cell, if it is ground, we set our data based on our ground condition
	if (isGroundGPU(x, y, z))
	{
		fillDataBoundCon(bounds.ground, sharedData[sharedIdx], dataVal, customDataVal);
	}
	else
	{
		sharedData[sharedIdx] = dataVal;
	}

	// If we are horizontally outside of the grid
	if (x != extraX)
	{
		//Offset sharedIdx and based on the type we set our data
		sharedIdx += extraIdxRight + extraIdxLeft;
		switch (horType)
		{
		case SKY: //Still inside boundary but outside block
			sharedData[sharedIdx] = data[getIdx(extraX, y, z)];
			break;
		case OUTSIDE:
			// Boundary condition for sides since we are looking horizontally
			fillDataBoundCon(bounds.sides, sharedData[sharedIdx], dataVal, customDataVal);
			break;
		case GROUND:
			fillDataBoundCon(bounds.ground, sharedData[sharedIdx], dataVal, customDataVal);
			break;
		default:
			break;
		}
		//Reset offset in case y is also offset
		sharedIdx -= extraIdxRight + extraIdxLeft;
	}
	if (y != extraY)
	{
		//Offset sharedIdx, and based on the type we set our data
		sharedIdx += (extraIdxUp + extraIdxDown) * (blockDim.x + 2);
		//CustomDataVal is not updated with new offset, but thats okay
		switch (verType)
		{
		case SKY:
			sharedData[sharedIdx] = data[getIdx(x, extraY, z)];
			break;
		case OUTSIDE:
			// Upwards boundary conditions since we are looking vertically
			fillDataBoundCon(bounds.up, sharedData[sharedIdx], dataVal, customDataVal);
			break;
		case GROUND:
			fillDataBoundCon(bounds.ground, sharedData[sharedIdx], dataVal, customDataVal);
			break;
		default:
			break;
		}
		//Reset offset in case of corner
		sharedIdx -= (extraIdxUp + extraIdxDown) * (blockDim.x + 2);
	}
	if (x != extraX && y != extraY)
	{
		// Handle Corner case
		//Offset sharedIdx, and based on the type we set our data
		sharedIdx += (extraIdxRight + extraIdxLeft) + (extraIdxUp + extraIdxDown) * (blockDim.x + 2);

		// GROUND has more priority, so we put it first
		envType cornType = SKY;
		if (horType == GROUND || verType == GROUND) cornType = GROUND;
		else if (horType == OUTSIDE || verType == OUTSIDE) cornType = OUTSIDE;

		switch (cornType)
		{
		case SKY: //Still inside boundary but outside block
			sharedData[sharedIdx] = data[getIdx(extraX, extraY, z)];
			break;
		case OUTSIDE:
			// Boundary condition for sides since we are looking horizontally
			fillDataBoundCon(bounds.sides, sharedData[sharedIdx], dataVal, customDataVal);
			break;
		case GROUND:
			fillDataBoundCon(bounds.ground, sharedData[sharedIdx], dataVal, customDataVal);
			break;
		default:
			break;
		}

	}
}

__device__ __forceinline__ float fillNeighbourData(envType neighbourType, boundsEnv condition, const float* data, const int idx, const int offset, const float customData, bool up)
{
	if (isOutside(idx))
	{
		printf("Warning: passed invalid index to fillNeighbourData\n");
		return 0.0f; //Invalid index to start with
	}
	
	float value = 0.0f;
	switch (neighbourType)
	{
	case SKY:
		value = data[idx + offset];
		break;
	case OUTSIDE:
		if (up) fillDataBoundCon(condition.up, value, data[idx], customData);
		else fillDataBoundCon(condition.sides, value, data[idx], customData);
		break;
	case GROUND:
		fillDataBoundCon(condition.ground, value, data[idx], customData);
		break;
	default:
		break;
	}
	return value;
}

__device__ __forceinline__ void fillDataBoundCon(boundCon condition, float& output, const float data, const float customData)
{
	switch (condition)
	{
	case NEUMANN:
		output = data;
		break;
	case DIRICHLET:
		output = 0.0f;
		break;
	case CUSTOM:
		output = customData;
		break;
	default:
		break;
	}
}

__device__ __forceinline__ envType getSafeNeighbourAtOffset(const Neigh* neigh, const int idx, const int offset, direction dir)
{
	const int offsetIdx = idx + offset;
	if (isOutside(idx + offset))
	{
		if (offsetIdx < 0) return GROUND;
		else return OUTSIDE;
	}

	switch (dir)
	{
	case LEFT:
		return neigh[offsetIdx].left;
		break;
	case RIGHT:
		return neigh[offsetIdx].right;
		break;
	case UP:
		return neigh[offsetIdx].up;
		break;
	case DOWN:
		return neigh[offsetIdx].down;
		break;
	case FORWARD:
		return neigh[offsetIdx].forward;
		break;
	case BACKWARD:
		return neigh[offsetIdx].backward;
		break;
	default:
		break;
	}

	return SKY;
}

