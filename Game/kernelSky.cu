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
	float forward = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forward, input[idx], defaultValue); // Fill forward if at ground
	else forward = input[idx];
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


		fillSharedNeigh(neigh, sharedBlock, input, defaultVal, z, bounds);
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


			l = (neigh[idx].left.type == GROUND && isGroundLevel(z)) ? (potentialTempGPU(groundT[idxGL] - 273.15f, pressuresAir[idx], groundP[idxGL]) + 273.15f) : sharedBlock[idxsData - 1];
			r = (neigh[idx].right.type == GROUND && isGroundLevel(z) ? (potentialTempGPU(groundT[idxGR] - 273.15f, pressuresAir[idx], groundP[idxGR]) + 273.15f) : sharedBlock[idxsData + 1]);
			d = (neigh[idx].down.type == GROUND) ? potentialTempGPU(groundT[idxG] - 273.15f, pressuresAir[idx], groundP[idxG]) + 273.15f : sharedBlock[idxsData - sharedBlockWidth];
			u = sharedBlock[idxsData + sharedBlockWidth]; // No ground upwards, so we can just use the shared block
			f = (neigh[idx].forward.type == GROUND && isGroundLevel(z)) ? (potentialTempGPU(groundT[idxGF] - 273.15f, pressuresAir[idx], groundP[idxGF]) + 273.15f) : forward;
			b = (neigh[idx].backward.type == GROUND && isGroundLevel(z)) ? (potentialTempGPU(groundT[idxGB] - 273.15f, pressuresAir[idx], groundP[idxGB]) + 273.15f) : backward;
			
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
	__shared__ float sharedBlockQrs[18 * 18];
	__shared__ float sharedBlockQgr[18 * 18];

	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int z = threadIdx.y + blockDim.y * blockIdx.y;
	const int idx = x + z * GRIDSIZESKYX;
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;
	int idxL = idx - 1;
	int idxR = idx + 1;
	int idxB = idx - GRIDSIZESKYX;
	int idxF = idx + GRIDSIZESKYX;

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
	if (x == GRIDSIZESKYX - 1)
	{
		sharedBlockQrs[idxsData + 1] = rsC;
		sharedBlockQgr[idxsData + 1] = grC;
		idxR = idx;
	}
	if (z == GRIDSIZESKYZ - 1)
	{
		sharedBlockQrs[idxsData + sharedBlockWidth] = rsC;
		sharedBlockQgr[idxsData + sharedBlockWidth] = grC;
		idxF = idx;
	}

	// Now, hold up
	__syncthreads();


	// Set neighbour values
	const float rsL = sharedBlockQrs[idxsData - 1];
	const float rsR = sharedBlockQrs[idxsData + 1];
	const float rsB = sharedBlockQrs[idxsData - sharedBlockWidth];
	const float rsF = sharedBlockQrs[idxsData + sharedBlockWidth];

	const float grL = sharedBlockQgr[idxsData - 1];
	const float grR = sharedBlockQgr[idxsData + 1];
	const float grB = sharedBlockQgr[idxsData - sharedBlockWidth];
	const float grF = sharedBlockQgr[idxsData + sharedBlockWidth];
		
	// Using helper Lambda function to calculate flow to cell from each direction
	// 'To' meaning the current cell and 'From' meaning the neighbouring
	auto flowSlope = [&](float heightFrom, float heightTo, float grFrom, float grTo, float rsFrom, float rsTo, float& outputGr, float& outputRs)
	{
		// Using Manning Formula: V = 1 / n * Rh^2/3 * S^1/2
		// We use for n = 0.030.
		// RH is weird for open way, so we use water depth, which is in our case is Qgr * 1 (in meters)
		const float n = 1 / 0.03f;

		// Hydraulic conductivity of the ground in m/s (How easy water flows through ground) https://structx.com/Soil_Properties_007.html
		const float k{ 5e-5f }; //Sand	

		// Divide by 1.0f is useless but its to say that height changed with n by 1 meters.
		const float slope = heightFrom - heightTo / 1.0f;

		const bool fromPrev = slope > 0.0f;
		const float slopeFromRain = fromPrev ? grFrom : grTo;
		const float slopeFromWater = fromPrev ? rsFrom : rsTo;

		float changeRain = 0.0f;

		if (slopeFromRain != 0.0f)
		{
			// Height of water
			const float RH = slopeFromRain;
			const float speed = n * powf(RH, 2.0f / 3.0f);

			// In m/s
			const float vel = speed * pow(slope, 0.5f);

			// Speed * time = distance, time will be included after limiting
			// Multiplying by slopeFromRain makes the water kind of stick the less it is, which is okay
			changeRain = vel * slopeFromRain;
		}
		const float changeWater = ConstantsGPU::g * k * slopeFromWater * slope;

		// Now diffusion
		const float DG = 1.75e-3f; // diffusion coefficient for ground rain water https://dtrx.de/od/diff/
		const float DS = 1.8e-5f; // diffusion coefficient for subsurface water https://www.researchgate.net/figure/Diffusion-coefficient-for-water-in-soils_tbl2_267235072

		const float diffusionR = 1 / (VOXELSIZE * VOXELSIZE) * DG;
		const float diffusionW = 1 / (VOXELSIZE * VOXELSIZE) * DS;

		const float lapR = (grFrom - grTo) * diffusionR;
		const float lapSR = (rsFrom - rsTo) * diffusionW;

		const bool fromPrevR = lapR > 0.0f;
		const bool fromPrevSR = lapSR > 0.0f;

		//Limit both outcomes, using abs to use the fmin correctly.
		const float flowGroundRain = dt * fmin(fabsf(lapR) * speed, fromPrevR ? grFrom : grTo) * (lapR > 0.0f ? 1.0f : -1.0f);
		const float flowGroundWater = dt * fmin(fabsf(lapSR) * speed, fromPrevSR ? rsFrom : rsTo) * (lapSR > 0.0f ? 1.0f : -1.0f);

		const float slopeFlowRain = dt * fmin(fabsf(changeRain) * speed, fromPrev ? grFrom : grTo) * (changeRain > 0.0f ? 1.0f : -1.0f);
		const float slopeFlowWater = dt * fmin(fabsf(changeWater) * speed, fromPrev ? rsFrom : rsTo) * (changeWater > 0.0f ? 1.0f : -1.0f);

		outputGr += flowGroundRain + slopeFlowRain;
		outputRs += flowGroundWater + slopeFlowWater;
	};
	
	// Go over all directions and add them
	float finalGr = grC, finalRs = rsC;
	const float heightC = float(GHeight[idx]);
	flowSlope(float(GHeight[idxL]), heightC, grL, grC, rsL, rsC, finalGr, finalRs);
	flowSlope(float(GHeight[idxR]), heightC, grR, grC, rsR, rsC, finalGr, finalRs);
	flowSlope(float(GHeight[idxB]), heightC, grB, grC, rsB, rsC, finalGr, finalRs);
	flowSlope(float(GHeight[idxF]), heightC, grF, grC, rsF, rsC, finalGr, finalRs);

	Qgr[idx] = finalGr;
	Qrs[idx] = finalRs;

	//if (Qgr[idx] < 0.0f) printf("x %i, z %i, Qgr[idx] %e, grL %e, grC %e, grB %e\n", x, z, Qgr[idx], grL, grC, grB);
}

__global__ void setTempsAtGroundGPU(float* potTemps, const float* groundTemps, const float* pressuresAir, const float* groundPressures, const float dt)
{
	const int x = threadIdx.x;
	const int z = blockIdx.x;
	const int Gidx = x + z * GRIDSIZESKYX; // Ground index
	const int y = GHeight[Gidx] + 1;
	const int idx = getIdx(x, y, z);

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

		fillSharedNeigh(neigh, sharedBlock, arrayIn, defaultVal, z, bounds);
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
			extraRight = getValueExtraDirShared(neigh, arrayIn, sharedBlock, idx, idxsData, 1, 1, RIGHT);
		}

		float valL = downWind ? sharedBlock[idxsData - 1] : extraRight;
		float valR = downWind ? sharedBlock[idxsData + 1] : sharedBlock[idxsData];
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
			extraLeft = getValueExtraDirShared(neigh, arrayIn, sharedBlock, idx, idxsData, -1, -1, LEFT);
		}


		valL = downWind ? sharedBlock[idxsData + 1] : extraLeft;
		valR = downWind ? sharedBlock[idxsData - 1] : sharedBlock[idxsData];
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

		fillSharedNeigh(neigh, sharedBlock, arrayIn, defaultVal, z, bounds);
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
			extraUp = getValueExtraDirShared(neigh, arrayIn, sharedBlock, idx, idxsData, GRIDSIZESKYX, sharedBlockWidth, UP);
		}

		float valD = downWind ? sharedBlock[idxsData - sharedBlockWidth] : extraUp;
		float valU = downWind ? sharedBlock[idxsData + sharedBlockWidth] : sharedBlock[idxsData];
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
			extraDown = getValueExtraDirShared(neigh, arrayIn, sharedBlock, idx, idxsData, -GRIDSIZESKYX, -sharedBlockWidth, DOWN);
		}

		valD = downWind ? sharedBlock[idxsData + sharedBlockWidth] : extraDown;
		valU = downWind ? sharedBlock[idxsData - sharedBlockWidth] : sharedBlock[idxsData];
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
	float forward = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forward, arrayIn[idx], defaultValue); // Fill forward if at ground
	else forward = arrayIn[idx];
	float current = fillNeighbourData(neigh[idx].backward, bounds, arrayIn, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultValue);
	float backward = current; // Can reuse current due to both being outside
	float backwardExtra = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// We do not return, we just continue 
		idx = getIdx(x, y, z);

		backwardExtra = backward;
		backward = current;
		current = forward;
		forward = forwardExtra;
		forwardExtra = getValueExtraForward(neigh, bounds, arrayIn, defaultValue, x, y, z);

		//if (z > 27 && y == 1 && x == 31) printf("x %i y: %i, z %i value: %f, value2 %f, final %f, FE %f, F %f, C %f, B %f, BE %f\n", x, y, z, 0.0f, 0.0f, (dt / VOXELSIZE) * (0.0f - 0.0f), forwardExtra, forward, current, backward, backwardExtra);
		if (isGroundGPU(x, y, z)) continue;

		// Downwind is when velocity is coming from the back
		// And since we are currently looking forward, downwind is moving with use thus positive
		bool downWind = velfieldZ[idx] >= 0.0f;

		float valB = downWind ? backward : forwardExtra;
		float valF = downWind ? forward : current;
		float valC = downWind ? current : forward;

		float fluxForward = advectPPMFlux(velfieldZ[idx], valB, valC, valF, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velB = fillNeighbourData(neigh[idx].backward, bounds, velfieldZ, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultVelZ[y]);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velB < 0.0f;

		valB = downWind ? forward : backwardExtra;
		valF = downWind ? backward : current;
		valC = downWind ? current : backward;

		float fluxBackward = advectPPMFlux(velB, valB, valC, valF, dt);


		arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxForward - fluxBackward);
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
		const bool up = !neigh[idx].up.outside;

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
		if (up)
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
		if (up)
		{
			Qj[idx] += dt * fmin((fallVelUp / VOXELSIZE) * QjU, QjU); //Grab % of precip above
		}
	}
}

__global__ void applyPreconditionerGPU(float* output, const float* precon, const float* div, float4* A)
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

__global__ void applyAGPU(float* output, const float* input, const Neigh* neigh, const float4* A, boundsEnv bounds)
{
	//Using shared data and forward + backward due to input need from all direction
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	float forward = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forward, input[idx], 0.0f); // Fill forward if at ground
	else forward = input[idx];
	float current = fillNeighbourData(neigh[idx].backward, bounds, input, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		idx = getIdx(x, y, z);

		const Neigh currentNeigh = neigh[idx];

		fillSharedNeigh(neigh, sharedBlock, input, nullptr, z, bounds);

		backward = current;
		current = forward;
		forward = fillNeighbourData(currentNeigh.forward, bounds, input, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);

		__syncthreads();

		// We do not return, since we still need to fill data
		bool isGround = isGroundGPU(x, y, z);

		float l = sharedBlock[idxsData - 1];
		float r = sharedBlock[idxsData + 1];
		float d = sharedBlock[idxsData - sharedBlockWidth];
		float u = sharedBlock[idxsData + sharedBlockWidth];
		float f = forward; //Useless, but to keep consistancy with naming
		float b = backward;

		// TODO: left and backwards 0 when not sky?
		char ALeft = (!currentNeigh.left.outside || currentNeigh.left.type == SKY) ? A[idx - 1].x : 0;
		char ADown = (!currentNeigh.down.outside || currentNeigh.down.type == SKY) ? A[idx - GRIDSIZESKYX].y : 0;
		char ABack = (!currentNeigh.backward.outside || currentNeigh.backward.type == SKY) ? A[idx - GRIDSIZESKYX * GRIDSIZESKYY].z : 0;
		float4 ACur = A[idx];


		output[idx] = isGround ? 0.0f : ACur.w * current +
			((ALeft * l +
				ADown * d +
				ABack * b +
				ACur.x * r +
				ACur.y * u +
				ACur.z * f)
				);
		//if (x == 16 && z == 0 && y == 16) printf("x %i, y %i, z %i, ACur.w: %i, current: %f, output[] %f, l %f,d %f,b %f,r %f,u %f,f %f,\n", x, y, z, ACur.w, current, output[idx], l, d, b, r, u, f);
	}
}

__global__ void calculateDivergenceGPU(float* divergence, const Neigh* neigh, const float* velX, const float* velY, const float* velZ, 
	const float* dens, const float* oldDens, const float* defaultDens, boundsEnv bounds, boundsEnv boundsVel, const float dt)
{
	// Only shared block for the density, since the velocity uses different arrays.
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;
	int idx = getIdx(x, y, z);

	float forward = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forward, dens[idx], 1.0f); // Fill forward if at ground
	else forward = dens[idx];
	float current = fillNeighbourData(neigh[idx].backward, bounds, dens, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 1.0f);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Be polite and wait a second.
		__syncthreads();

		idx = getIdx(x, y, z);

		fillSharedNeigh(neigh, sharedBlock, dens, defaultDens, z, bounds);

		backward = current;
		current = forward;
		forward = fillNeighbourData(neigh[idx].forward, bounds, dens, idx, GRIDSIZESKYX * GRIDSIZESKYY, 1.0f);

		__syncthreads();

		// Velocities
		float ul = 0.0f, ur = 0.0f, ud = 0.0f, uu = 0.0f, uf = 0.0f, ub = 0.0f;
		// Densities
		float dl = 0.0f, dr = 0.0f, dd = 0.0f, du = 0.0f, df = 0.0f, db = 0.0f;

		//if (z < 3 && y == 1) printf("x %i, y %i, z %i, outsideForward = %i, typeForward %i, backward %f, current %f, forward %f, oldDens %f\n", x, y, z, neigh[idx].forward.outside, neigh[idx].forward.type, backward, current, forward, oldDens[idx]);
		divergence[idx] = 0.0f;

		// We do not return, since we still need to fill data
		if (isGroundGPU(x, y, z)) continue;

		ur = velX[idx];
		ul = fillNeighbourData(neigh[idx].left, boundsVel, velX, idx, -1, defaultVelX[y]);
		uu = velY[idx];
		ud = fillNeighbourData(neigh[idx].down, boundsVel, velY, idx, -GRIDSIZESKYX, 0.0f);
		uf = velZ[idx];
		ub = fillNeighbourData(neigh[idx].backward, boundsVel, velZ, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultVelZ[y]);

		// Get correct densities
		dr = sharedBlock[idxsData + 1];
		dl = sharedBlock[idxsData - 1];
		du = sharedBlock[idxsData + sharedBlockWidth];
		dd = sharedBlock[idxsData - sharedBlockWidth];
		df = forward;
		db = backward;

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
		//if (z < 3 && y == 1) printf("x %i, y %i, z %i, massFluxDiv = %f, densChange %f, backward %f, current %f, forward %f, oldDens %f\n", x, y, z, massFluxDiv, densityChange, backward, current, forward, oldDens[idx]);
		//if (z > 29 && y == 1) printf("x %i, y %i, z %i, massFluxDiv = %f, densChange %f, dr %f,dl %f,du %f,dd %f,df %f,db %f\n", x, y, z, massFluxDiv, densityChange, ur, ul, uu, ud, uf, ub);
	}
}

__global__ void applyPresProjGPU(const float* pressure, const Neigh* neigh, float* velX, float* velY, float* velZ, const float* density,const float* pressureEnv, boundsEnv boundsDens, const float dt)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;

	// Neumann means no air flowing in or out due to doing - currentP at the end
	// Dirichlet means air flowing in or out
	boundsEnv boundsPresProj{ DIRICHLET, NEUMANN, NEUMANN };


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
		pr = fillNeighbourData(neigh[idx].right, boundsPresProj, pressure, idx, 1, 0.0f);
		pu = fillNeighbourData(neigh[idx].up, boundsPresProj, pressure, idx, GRIDSIZESKYX, 0.0f);
		pf = fillNeighbourData(neigh[idx].forward, boundsPresProj, pressure, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
		

		//Density at face
		dr = fillNeighbourData(neigh[idx].right, boundsDens, density, idx, 1, 0.0f);
		du = fillNeighbourData(neigh[idx].up, boundsDens, density, idx, GRIDSIZESKYX, 0.0f);
		df = fillNeighbourData(neigh[idx].forward, boundsDens, density, idx, GRIDSIZESKYX * GRIDSIZESKYY, 0.0f);
		// Calculate harmonic mean
		dr = dr == 0.0f ? currentD : 2.0f / (1.0f / currentD + 1.0f / dr);
		du = du == 0.0f ? currentD : 2.0f / (1.0f / currentD + 1.0f / du);
		df = df == 0.0f ? currentD : 2.0f / (1.0f / currentD + 1.0f / df);

		//Using dt to match dt used in divergence calculation
		//Dividing by density at cell faces gives for pressure effects
		velX[idx] += (pr - currentP) / dr;
		velY[idx] += (pu - currentP) / du;
		velZ[idx] += (pf - currentP) / df;

		//if (x == 31 && z == 16) printf("x %i, y %i, z %i, pressure[%i] = %e, pr %e, pu %e, pf %e, currentP %e, outside: %i\n", x, y, z, idx, pressure[idx], pr, pu, pf, currentP, neigh[idx].up.outside);
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

		if (*S2 == 0.0f) printf("ERROR: S2 = 0.0f in updatePandDiv\n");
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
		if (!isGroundGPU(x, y, z))
		{
			const int idx = getIdx(x, y, z);
			envPressure[idx] -= presProj[idx] / 100.0f; //Pa to Pha
		}
	}
}

//-------------------------------------OTHER-------------------------------------

__global__ void calculateCloudCoverGPU(float* output, const float* Qc, const float* Qw, const int* GHeight)
{
	const int x = threadIdx.x;
	const int z = blockIdx.x;
	const int idxG = x + z * GRIDSIZESKYX;

	float totalCloudContent = 0.0f;
	const float qfull = 1.2f; // a threshold value where all incoming radiation is reflected by cloud matter: http://meto.umd.edu/~zli/PDF_papers/Li%20Nature%20Article.pdf
	for (int y = GHeight[idxG] + 1; y < GRIDSIZESKYY; y++) totalCloudContent += (Qc[getIdx(x, y, z)] + Qw[getIdx(x, y, z)]) * VOXELSIZE;
	output[idxG] = fmin(totalCloudContent / qfull, 1.0f);
}

__global__ void calculateGroundTempGPU(float* groundT, const float dtSpeed, const float irridiance, const float* LC)
{
	const int x = threadIdx.x;
	const int z = blockIdx.x;
	const int idxG = x + z * GRIDSIZESKYX;

	const float groundTemp = groundT[idxG];
	const float absorbedRadiationAlbedo = 0.25f;  //How much light is reflected back? 0 = absorbes all, 1 = reflects all
	const float groundThickness = 1.0f; //Just used 1 meter
	const float densityGround = 1500.0f;
	const double T4 = groundTemp * groundTemp * groundTemp * groundTemp;

	groundT[idxG] += dtSpeed * ((1 - LC[idxG]) * (((1 - absorbedRadiationAlbedo) * irridiance - ConstantsGPU::ge * ConstantsGPU::oo * T4) / (groundThickness * densityGround * ConstantsGPU::Cpds)));
}

__global__ void buoyancyGPU(float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* defTemp, const float* defQv, const float* pressures, const float* groundP, float* buoyancyStor, const float , boundsEnv bounds, boundsEnv boundsVelY, const float dt)
{
	__shared__ float sharedBlockQv[18 * 18];
	__shared__ float sharedBlockT[18 * 18];

	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;

	float defaultValueTemp = defTemp ? defTemp[y] : 0.0f;
	float defaultValueQv = defQv ? defQv[y] : 0.0f;


	float forwardQv = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forwardQv, Qv[idx], defaultValueQv); // Fill forward if at ground
	else forwardQv = Qv[idx];
	float forwardT = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forwardT, potTemp[idx], defaultValueTemp); // Fill forward if at ground
	else forwardT = potTemp[idx];
	float currentQv = fillNeighbourData(neigh[idx].backward, bounds, Qv, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultValueQv);
	float currentT = fillNeighbourData(neigh[idx].backward, bounds, potTemp, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultValueTemp);
	float backwardQv = 0;
	float backwardT = 0;


	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();
		idx = getIdx(x, y, z);

		float currentVelY = velY[idx];// getVelAtIdx(neigh, boundsVelY, DOWN, velY, 0.0f, idx);

		bool up = !neigh[idx].up.outside;
		bool down = !(neigh[idx].down.outside || neigh[idx].down.type != SKY);

		backwardQv = currentQv;
		backwardT = currentT;
		currentQv = forwardQv;
		currentT = forwardT;
		forwardQv = fillNeighbourData(neigh[idx].forward, bounds, Qv, idx, GRIDSIZESKYX * GRIDSIZESKYY, defaultValueQv);
		forwardT = fillNeighbourData(neigh[idx].forward, bounds, potTemp, idx, GRIDSIZESKYX * GRIDSIZESKYY, defaultValueTemp);
		const float groundPs = groundP[x + z * GRIDSIZESKYX];
		const float	psC = pressures[idx];
		const float psU = up ? pressures[idx + GRIDSIZESKYX] : psC;
		const float psD = down ? pressures[idx - GRIDSIZESKYX] : psC;

		fillSharedNeigh(neigh, sharedBlockQv, Qv, defQv, z, bounds);
		fillSharedNeigh(neigh, sharedBlockT, potTemp, defTemp, z, bounds);
		__syncthreads();

		// We do not return, we just continue 
		if (isGroundGPU(x, y, z)) continue;

		// If the type is a solid, we do not want to account for it
		const bool l = !(bounds.sides == DIRICHLET && neigh[idx].left.outside) && neigh[idx].left.type == SKY;
		const bool r = !(bounds.sides == DIRICHLET && neigh[idx].right.outside) && neigh[idx].right.type == SKY;
		const bool b = !(bounds.sides == DIRICHLET && neigh[idx].backward.outside) && neigh[idx].backward.type == SKY;
		const bool f = !(bounds.sides == DIRICHLET && neigh[idx].forward.outside) && neigh[idx].forward.type == SKY;
		const int validEnv = int(l) + int(r) + int(b) + int(f);

		//if (z > 29 && y == 1) printf("x %i, y %i, z %i, forwardT %f, currentT %f, backwardT %f, forwardQv %f, currentQv %f, backwardQv %f\n", x, y, z, forwardT, currentT, backwardT, forwardQv, currentQv,backwardQv);


		// ------------ QV ------------

		//Vapor environment and Vapor Parcel
		float Qenv = 0.0f, QenvUp = 0.0f, QenvDown = 0.0f;
		float QP = 0.0f;

		QP = sharedBlockQv[idxsData];

		Qenv = (sharedBlockQv[idxsData + 1] + sharedBlockQv[idxsData - 1] + forwardQv + backwardQv) / validEnv;
		QenvUp = sharedBlockQv[idxsData + sharedBlockWidth];
		QenvDown = sharedBlockQv[idxsData - sharedBlockWidth];


		// ------------ Temp ------------

		const float TDown = sharedBlockT[idxsData - sharedBlockWidth] * powf(psD / groundPs, ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float T = sharedBlockT[idxsData] * powf(psC / groundPs, ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float TUp = sharedBlockT[idxsData + sharedBlockWidth] * powf(psU / groundPs, ConstantsGPU::Rsd / ConstantsGPU::Cpd);

		// Temp for up and down based on adiabatics
		float Tadiab = sharedBlockT[idxsData], TadiabUp = sharedBlockT[idxsData], TadiabDown = sharedBlockT[idxsData];
		// Environment Temp
		float Tenv = 0.0f, TenvUp = 0.0f, TenvDown = 0.0f;
		Tenv = (sharedBlockT[idxsData + 1] + sharedBlockT[idxsData - 1] + forwardT + backwardT) / validEnv;
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

		const float totalQ = Qr[idx] + Qs[idx] + Qi[idx];
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
		if (currentVelY > 0.0f) {
			buoyancyFinal = BUp; //Going up, thus use the buoyancy from above, if it is much warmer, this will be negative, meaning descending of parcel
		}
		else if (currentVelY < 0.0f) {
			buoyancyFinal = BDown; //Going up, thus buoyancy from below, if it is much colder, this will be positive, meaning rising of parcel again
		}
		//if (z < 5 && x == 16 && y == 16) printf("x %i, y %i, z %i, buoyancyFinal %f, Tadiab %f, Tenv %f, QP %f, Qenv %f, Tenv1 %f, Tenv-1 %f, forwardT %f, backwardT %f, validEnv %i\n", x, y, z, buoyancyFinal, TadiabUp, TenvUp, QP, QenvUp, sharedBlockT[idxsData + 1], sharedBlockT[idxsData - 1], forwardT, backwardT, validEnv);

		// With the correct buoyancy applied we could just insert it into the velocity, yet this causes a lot of back and forwarding.
		// Instead, we limit this back and forwarding
		// When we change the velocity due to buoyancy too much (going from positive to negative of visa versa), we just set velocity to 0.
		// This makes the air much more stable

		float change = buoyancyFinal * dt;
		if ((up && currentVelY > 0.0f && -change > currentVelY) || (down && currentVelY < 0.0f && -change < currentVelY))
		{
			change = -velY[idx]; //TODO: remove velY or currentVelY? Probably velY to make cap stronger
		}

		//if (z < 15 && x == 16 && y == 16) printf("x %i, y %i, z %i, up %i, down %i, velY %f, change %f\n", x, y, z, up, down, velY[idx], change);

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
		const bool currentG = isGroundGPU(x, y, z);

		// Set all outside bools
		// And set types to be GROUND if they are ground OR with all but up, when current is ground

		Neigh[idx].left.outside = x == 0;
		Neigh[idx].left.type = (Neigh[idx].left.outside && currentG) || (!Neigh[idx].left.outside && isGroundGPU(x - 1, y, z)) ? GROUND : SKY;

		Neigh[idx].right.outside = x == GRIDSIZESKYX - 1;
		Neigh[idx].right.type = (Neigh[idx].right.outside && currentG) || (!Neigh[idx].right.outside && isGroundGPU(x + 1, y, z)) ? GROUND : SKY;

		Neigh[idx].down.outside = y == 0;
		Neigh[idx].down.type = (Neigh[idx].down.outside && currentG) || (!Neigh[idx].down.outside && isGroundGPU(x, y - 1, z)) ? GROUND : SKY;

		Neigh[idx].up.outside = y == GRIDSIZESKYY - 1;
		Neigh[idx].up.type = !Neigh[idx].up.outside && isGroundGPU(x, y + 1, z) ? GROUND : SKY;

		Neigh[idx].backward.outside = z == 0;
		Neigh[idx].backward.type = (Neigh[idx].backward.outside && currentG) || (!Neigh[idx].backward.outside && isGroundGPU(x, y, z - 1)) ? GROUND : SKY;

		Neigh[idx].forward.outside = z == GRIDSIZESKYZ - 1;
		Neigh[idx].forward.type = (Neigh[idx].forward.outside && currentG) || (!Neigh[idx].forward.outside && isGroundGPU(x, y, z + 1)) ? GROUND : SKY;
	}
}



__global__ void initAMatrix(float4* A, const Neigh* neigh, const float* density, const float* defDens, boundsEnv bounds)
{
	__shared__ float sharedBlock[18 * 18];
	const int sharedBlockWidth = blockDim.x + 2;
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	// Already set forward and current to the position where they can get easily swapped in the for loop
	float forward = 0.0f;
	if (isGroundGPU(x, y, z)) fillDataBoundCon(bounds.ground, forward, density[idx], 1.0f); // Fill forward if at ground
	else forward = density[idx];
	float current = fillNeighbourData(neigh[idx].backward, bounds, density, idx, -GRIDSIZESKYX * GRIDSIZESKYY, 1.0f);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// Early birds can wait
		__syncthreads();
		idx = getIdx(x, y, z);

		backward = current;
		current = forward;
		forward = fillNeighbourData(neigh[idx].forward, bounds, density, idx, GRIDSIZESKYX * GRIDSIZESKYY, 1.0f);

		fillSharedNeigh(neigh, sharedBlock, density, defDens, z, bounds);
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

		//if (A[idx].w < 6) printf("x %i, y %i, z %i, A.x %f, A.y %f, A.z %f, A.w %f\n", x, y, z, A[idx].x, A[idx].y, A[idx].z, A[idx].w);
	}
}

__global__ void initPrecon(float* precon, const float4* A)
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
		if (!isGroundGPU(x, y, z))
		{
			const int idxG = x + z * GRIDSIZESKYX;

			const float T = float(potTemp[idx]) * glm::pow(pressures[idx] / groundP[idxG], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			const float Tv = T * (0.608f * Qv[idx] + 1);
			const float density = pressures[idx] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa

			densityAir[idx] = density;
		}
	}
}

__global__ void calculateNewPressure(float* pressureEnv, const float* densityAir, const float* potTemp, const float* Qv, const float* GPressure)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdx(x, y, z);
		if (!isGroundGPU(x, y, z))
		{
			const int idxG = x + z * GRIDSIZESKYX;

			//Yes, we use pressure to first calculate T and then calculate pressure, it is double sided but how else?
			const float T = float(potTemp[idx]) * glm::pow(pressureEnv[idx] / GPressure[idxG], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			const float Tv = T * (0.608f * Qv[idx] + 1);
			pressureEnv[idx] = densityAir[idx] * ConstantsGPU::Rsd * Tv / 100.0f;
		}
	}

}

__global__ void applyBrushGPU(float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const float brushSize, const int3 position, 
	const float brushSmoothness, const float brushIntensity, const float applyValue, const float3 valueDir, const bool groundErase, const float dt)
{
	//Thread
	const int x = threadIdx.x;
	const int y = blockIdx.x;


	for (int z = 0; z < GRIDSIZESKYZ; z++)
	{

		const int brushOffset = int(ceilf(brushSize));
		//Local X and Y (is offset half to the bottom left)
		const int lX = x - brushOffset;
		const int lY = y - brushOffset;
		const int lZ = z - brushOffset;

		//Including mouse and correct brush offset
		const int mX = x + position.x - brushOffset;
		const int mY = y + position.y - brushOffset;
		const int mZ = z + position.z - brushOffset;
		const int mIdx = getIdx(mX, mY, mZ);

		if (mX >= GRIDSIZESKYX || mY >= GRIDSIZESKYY || mZ > GRIDSIZESKYZ || mX < 0 || mY < 0 || mZ < 0) return;

		//Can't brush the ground
		if (paramType != PGROUND && mY <= GHeight[mX + mZ * GRIDSIZESKYX]) return;

		//Distance check
		const float eX = lX;
		const float eY = lY;
		const float eZ = lZ;
		const float distance = eX * eX + eY * eY + eZ * eZ; //This works since we determine from 0.
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
			float value3 = 0.0f;
			if (paramType == 7)
			{
				value3 = value1 * valueDir.z;
				value2 = value1 * valueDir.y;
				value1 *= valueDir.x;
			}

			//Apply
			if (paramType == PGROUND) //For ground
			{
				const bool groundChanged = setGround(groundGridStor, mX, mY, mZ, groundErase);
				if (groundChanged) *changedGround = groundChanged; //We don;t want to set back to false, if ever set, it is that.
			}
			else if (paramType == WIND) //For wind
			{
				array[mIdx] += value1;
				array2[mIdx] += value2;
				array3[mIdx] += value3;
			}
			else
			{
				array[mIdx] += value1;
			}
		}
	}
}

__global__ void applySelectionGPU(float* array, float* array2, float* array3, int* groundGridStor, bool* changedGround, parameter paramType, const int3 minPos, const int3 maxPos, const float applyValue, const float3 valueDir, const bool groundErase)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;

	for (int z = minPos.z; z <= maxPos.z; z++)
	{
		//Add offset to the thread
		const int mX = x + minPos.x;
		const int mY = y + minPos.y;
		const int mZ = z;
		const int mIdx = getIdx(mX, mY, mZ);

		if (mX >= GRIDSIZESKYX || mY >= GRIDSIZESKYY || mZ > GRIDSIZESKYZ || mX < 0 || mY < 0 || mZ < 0) return;
		//Can't brush the ground
		if (paramType != PGROUND && mY <= GHeight[mX + mZ * GRIDSIZESKYX]) return;

		//For directional value
		float value1 = applyValue;
		float value2 = 0.0f;
		float value3 = 0.0f;
		if (paramType == 7)
		{
			value3 = value1 * valueDir.z;
			value2 = value1 * valueDir.y;
			value1 *= valueDir.x;
		}

		//Apply
		if (paramType == PGROUND) //For ground
		{
			const bool groundChanged = setGround(groundGridStor, mX, mY, mZ, groundErase);
			if (groundChanged) *changedGround = groundChanged; //We don;t want to set back to false, if ever set, it is that.
		}
		else if (paramType == WIND) //For wind
		{
			array[mIdx] = value1;
			array2[mIdx] = value2;
			array3[mIdx] = value3;
		}
		else
		{
			array[mIdx] = value1;
		}
	}
}

__device__ bool setGround(int* groundHeight, const int x, const int y, const int z, const bool eraseGround)
{

	if (eraseGround && y <= GHeight[x + z * GRIDSIZESKYX])
	{
		int oldVal = atomicMin(&groundHeight[x + z * GRIDSIZESKYX], y);
		return true;
	}
	else if (!eraseGround && y > GHeight[x + z * GRIDSIZESKYX])
	{
		int oldVal = atomicMax(&groundHeight[x + z * GRIDSIZESKYX], y);
		return true;
	}
	return false;
}

__global__ void compareAndResetValuesOutGround(const int* oldGroundHeight, const int* newGroundHeight, const float* isentropicTemp, const float* isentropicVap, 
	float* Qv, float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* potTemp, float* velX, float* velY, float* velZ, float* pres, float* defaultPres)
{
	const int x = threadIdx.x;
	const int y = blockIdx.x;

	for (int z = 0; z < GRIDSIZESKYZ; z++)
	{
		const int idx = getIdx(x, y, z);
		const int idxG = x + z * GRIDSIZESKYX;

		//return if Y is not at in between the old and new ground height

		if (oldGroundHeight[idxG] != newGroundHeight[idxG]) printf("x %i, y %i, z %i, old %i, new %i\n", x, y, z, oldGroundHeight[idxG], newGroundHeight[idxG]);
		if (y > oldGroundHeight[idxG] || y <= newGroundHeight[idxG]) return;
		Qv[idx] = isentropicVap[y];
		Qw[idx] = 0.0f;
		Qc[idx] = 0.0f;
		Qr[idx] = 0.0f;
		Qs[idx] = 0.0f;
		Qi[idx] = 0.0f;
		potTemp[idx] = isentropicTemp[y];
		velX[idx] = 0.0f;
		velY[idx] = 0.0f;
		velZ[idx] = 0.0f;
		pres[idx] = defaultPres[y];
	}
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

		if (neigh[idx].up.outside || neigh[idx].up.type != SKY)
		{
			velY[idx] = 0.0f;
		}

		if (neigh[idx].right.outside)
		{
			velX[idx] = defaultVelX[y];
		}
		if (neigh[idx].right.type != SKY)
		{
			velX[idx] = 0.0f;
		}
		if (neigh[idx].forward.outside)
		{
			velZ[idx] = defaultVelZ[y];
		}
		if (neigh[idx].forward.type != SKY)
		{
			velZ[idx] = 0.0f;
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


__device__ __forceinline__ float getVelAtIdx(const Neigh* neigh, boundsEnv bounds, direction dir, const float* vel, const float customData, const int idx)
{
	singleNeigh neighbour;
	int offset = 0;
	// Left and Right are the same due to right being the current index
	switch (dir)
	{
	case LEFT:
	case RIGHT:
		neighbour = neigh[idx].left;
		offset = -1;
		break;
	case UP:
	case DOWN:
		neighbour = neigh[idx].down;
		offset = -GRIDSIZESKYX;
		break;
	case FORWARD:
	case BACKWARD:
		neighbour = neigh[idx].backward;
		offset = -GRIDSIZESKYX * GRIDSIZESKYY;
		break;
	default:
		neighbour = neigh[idx].down;
		offset = -GRIDSIZESKYX;
		break;
	}
	
	float current = vel[idx];
	float other = fillNeighbourData(neighbour, bounds, vel, idx, offset, customData);

	return (current + other) * 0.5f;
}

__device__ void fillSharedNeigh(const Neigh* neigh, float* sharedData, const float* data, const float* customData, const int z, boundsEnv bounds)
{
	const int x = threadIdx.x + blockDim.x * blockIdx.x;
	const int y = threadIdx.y + blockDim.y * blockIdx.y;
	const int idx = getIdx(x, y, z);

	float customDataVal = customData ? customData[y] : 0.0f;
	float dataVal = data[idx];
	// We add offset since our halo is 18x18
	const int sharedBlockWidth = blockDim.x + 2;
	int sharedIdx = threadIdx.x + 1 + (threadIdx.y + 1) * sharedBlockWidth;


	// We can already set our current cell, if it is ground, we set our data based on our ground condition
	if (isGroundGPU(x, y, z))
	{
		fillDataBoundCon(bounds.ground, sharedData[sharedIdx], dataVal, customDataVal);
	}
	else
	{
		sharedData[sharedIdx] = dataVal;
	}

	// Now we need to make sure the threads at the edges set the values of their neighbours

	const bool left = threadIdx.x == 0;
	const bool right = threadIdx.x == blockDim.x - 1;
	const bool down = threadIdx.y == 0;
	const bool up = threadIdx.y == blockDim.y - 1;

	// At left side of block
	if (left)
	{
		sharedData[sharedIdx - 1] = fillNeighbourData(neigh[idx].left, bounds, data, idx, -1, customDataVal);
	}
	if (right)
	{
		sharedData[sharedIdx + 1] = fillNeighbourData(neigh[idx].right, bounds, data, idx, 1, customDataVal);
	}
	if (down)
	{
		sharedData[sharedIdx - sharedBlockWidth] = fillNeighbourData(neigh[idx].down, bounds, data, idx, -GRIDSIZESKYX, customDataVal);
	}
	if (up)
	{
		sharedData[sharedIdx + sharedBlockWidth] = fillNeighbourData(neigh[idx].up, bounds, data, idx, GRIDSIZESKYX, customDataVal);
	}

	singleNeigh sNeighbour;
	

	// Now to set the corners
	if (left && down)
	{
		// Make sure to set outside value to true if one of the neighbours are outside
		sNeighbour.outside = neigh[idx].down.outside || neigh[idx].left.outside;
		sNeighbour.type = neigh[idx].down.type;
		sharedData[sharedIdx - 1 - sharedBlockWidth] = fillNeighbourData(sNeighbour, bounds, data, idx, -1 - GRIDSIZESKYX, customDataVal);
	}
	else if (right && down)
	{
		sNeighbour.outside = neigh[idx].down.outside || neigh[idx].right.outside;
		sNeighbour.type = neigh[idx].down.type;
		sharedData[sharedIdx + 1 - sharedBlockWidth] = fillNeighbourData(sNeighbour, bounds, data, idx, 1 - GRIDSIZESKYX, customDataVal);
	}
	else if (right && up)
	{
		sNeighbour.outside = neigh[idx].up.outside || neigh[idx].right.outside;
		sNeighbour.type = neigh[idx].right.type;
		sharedData[sharedIdx + 1 + sharedBlockWidth] = fillNeighbourData(sNeighbour, bounds, data, idx, 1 + GRIDSIZESKYX, customDataVal);
	}
	else if (left && up)
	{
		sNeighbour.outside = neigh[idx].up.outside || neigh[idx].left.outside;
		sNeighbour.type = neigh[idx].left.type;
		sharedData[sharedIdx - 1 + sharedBlockWidth] = fillNeighbourData(sNeighbour, bounds, data, idx, -1 + GRIDSIZESKYX, customDataVal);
	}
}

__device__ __forceinline__ float fillNeighbourData(singleNeigh neighbourType, boundsEnv condition, const float* data, const int idx, const int offset, const float customData, bool up)
{
	if (isOutside(idx))
	{
		printf("Warning: passed invalid index to fillNeighbourData\n");
		return 0.0f; //Invalid index to start with
	}
	
	float value = 0.0f;
	if (neighbourType.outside)
	{
		if (up) fillDataBoundCon(condition.up, value, data[idx], customData);
		else if (neighbourType.type == SKY) fillDataBoundCon(condition.sides, value, data[idx], customData);
		else if (neighbourType.type == GROUND) fillDataBoundCon(condition.ground, value, data[idx], customData);
	}
	else
	{
		switch (neighbourType.type)
		{
		case SKY:
			value = data[idx + offset];
			break;
		case GROUND:
			fillDataBoundCon(condition.ground, value, data[idx], customData);
			break;
		default:
			break;
		}
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

__device__ __forceinline__ float getValueExtraDirShared(const Neigh* neigh, const float* data, const float* sharedData, const int idx, const int idxS, const int offset, const int offsetS, direction dir)
{
	float value = 0.0f;

	bool outsideDir = false;

	switch (dir)
	{
	case LEFT:
		outsideDir = neigh[idx].left.outside;
		break;
	case RIGHT:
		outsideDir = neigh[idx].right.outside;
		break;
	case UP:
		outsideDir = neigh[idx].up.outside;
		break;
	case DOWN:
		outsideDir = neigh[idx].down.outside;
		break;
	default:
		break;
	}

	if (outsideDir)
	{
		value = sharedData[idxS + 1 * offsetS];
	}
	else
	{
		outsideDir = false;
		switch (dir)
		{
		case LEFT:
			outsideDir = neigh[idx + 1 * offset].left.outside;
			break;
		case RIGHT:
			outsideDir = neigh[idx + 1 * offset].right.outside;
			break;
		case UP:
			outsideDir = neigh[idx + 1 * offset].up.outside;
			break;
		case DOWN:
			outsideDir = neigh[idx + 1 * offset].down.outside;
			break;
		default:
			break;
		}

		if (outsideDir)
		{
			value = sharedData[idxS + 2 * offsetS]; // Should be safe accessing this data due to block being 16 wide
		}
		else
		{
			bool safeShare = false;

			// Based on direction, check if safe to use shared data
			switch (dir)
			{
			case LEFT:
				safeShare = threadIdx.x > 0;
				break;
			case RIGHT:
				safeShare = threadIdx.x + 1 < blockDim.x;
				break;
			case UP:
				safeShare = threadIdx.y + 1 < blockDim.y;
				break;
			case DOWN:
				safeShare = threadIdx.y > 0;
				break;
			default:
				break;
			}

			if (safeShare)
			{
				value = sharedData[idxS + 2 * offsetS]; // Still safe to use shared data
			}
			else
			{
				value = data[idx + 2 * offset]; // We really have to access data :<
			}
		}
	}
	return value;
}

__device__ __forceinline__ float getValueExtraForward(const Neigh* neigh, boundsEnv bounds, const float* data, const float customData, const int x, const int y, const int z)
{
	int idx = getIdx(x, y, z);

	float value = 0.0f; 

	// If next one is outside, meaning extra forward is also outside
	if (neigh[idx].forward.outside) 
	{
		if (neigh[idx].forward.type == GROUND) // Meaning current is ground and outside it thus marked as ground
		{
			if (bounds.ground == NEUMANN) printf("Warning: Neumann at ground is not handled correctly with forward, current and backward\n");
			
			fillDataBoundCon(bounds.ground, value, data[idx], customData);
		}
		else
		{
			fillDataBoundCon(bounds.sides, value, data[idx], customData);
		}
	}
	else
	{
		idx += GRIDSIZESKYX * GRIDSIZESKYY;

		// We now can safely access the next forward
		if (neigh[idx].forward.outside)
		{
			if (neigh[idx].forward.type == GROUND) // Meaning current is ground and outside it thus marked as ground
			{
				if (bounds.ground == NEUMANN) printf("Warning: Neumann at ground is not handled correctly with forward, current and backward\n");

				fillDataBoundCon(bounds.ground, value, data[idx], customData);
			}
			else
			{

				fillDataBoundCon(bounds.sides, value, data[idx], customData);
			}
		}
		else
		{

			// Target is inside
			if (neigh[idx].forward.type == GROUND)
			{
				if (bounds.ground == NEUMANN) printf("Warning: Neumann at ground is not handled correctly with forward, current and backward\n");

				fillDataBoundCon(bounds.ground, value, data[idx], customData);
			}
			else
			{
				// Finally, the target is inside and not ground
				value = data[idx + GRIDSIZESKYX * GRIDSIZESKYY];

				//if (z > 27 && y == 1 && x == 31) printf("x %i y: %i, z %i value: %f\n", x, y, z, value);
			}
		}
	}
	return value;
}

