#include "kernelSky.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "utils.cuh"
#include "environment.cuh"
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
	const int blockWidth = blockDim.x;
	int x = threadIdx.x + blockWidth * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	// Already set forward and current to the position where they can get easily swapped in the for loop
	float forward = input[idx];
	float current = 0.0f;
	fillDataBoundCon(bounds.sides, current, input[idx], defaultVal[idx]);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		backward = current;
		current = forward;
		if (neigh[idx].forward == SKY) forward = input[getIdx(x, y, z + 1)];
		else if (neigh[idx].forward == GROUND) fillDataBoundCon(bounds.ground, forward, input[idx], defaultVal[idx]);
		else fillDataBoundCon(bounds.sides, forward, input[idx], defaultVal[idx]); 

		idx = getIdx(x, y, z);

		fillSharedNeigh(sharedBlock, input, defaultVal, z, bounds);
		__syncthreads();

		float l = 0.0f, r = 0.0f, d = 0.0f, u = 0.0f, f = 0.0f, b = 0.0f;
		const int idxsData = threadIdx.x + threadIdx.y * blockWidth;

		// We do not return, since we still need to fill data
		if (red && (x + y + z) % 2 == 0 || isGroundGPU(x, y, z)) continue;

		// We use custom boundary conditions on ground for temperature,
		// This is because we do not save potential temp on the ground
		if (type == 0)
		{
			const int idxG = x + z * GRIDSIZESKYX;
			const int idxGL = x > 0 ? idxG - 1 : idxG;
			const int idxGR = x + 1 < GRIDSIZESKYX ? idxG + 1 : idxG;
			const int idxGF = z + 1 < GRIDSIZESKYZ ? idxG + GRIDSIZESKYX : idxG;
			const int idxGB = z > 0 ? idxG - GRIDSIZESKYX : idxG;

			l = (neigh[idx].left == GROUND && isGroundLevel()) ? (potentialTempGPU(groundT[idxGL] - 273.15f, pressuresAir[idx], groundP[idxGL]) + 273.15f) : sharedBlock[idxsData - 1];
			r = (neigh[idx].right == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGR] - 273.15f, pressuresAir[idx], groundP[idxGR]) + 273.15f) : sharedBlock[idxsData + 1]);
			d = (neigh[idx].down == GROUND) ? potentialTempGPU(groundT[idxG] - 273.15f, pressuresAir[idx], groundP[idxG]) + 273.15f : sharedBlock[idxsData - blockWidth];
			u = sharedBlock[idxsData + blockWidth]; // No ground upwards, so we can just use the shared block
			f = (neigh[idx].forward == GROUND && isGroundLevel()) ? (potentialTempGPU(groundT[idxGF] - 273.15f, pressuresAir[idx], groundP[idxGF]) + 273.15f) : forward;
			b = (neigh[idx].backward == GROUND && isGroundLevel()) ? (potentialTempGPU(groundT[idxGB] - 273.15f, pressuresAir[idx], groundP[idxGB]) + 273.15f) : backward;
		}
		else
		{
			l = sharedBlock[idxsData - 1];
			r = sharedBlock[idxsData + 1];
			d = sharedBlock[idxsData - blockWidth];
			u = sharedBlock[idxsData + blockWidth];
			f = forward;
			b = backward;
		}

		output[idx] = (input[idx] + k * (l + r + u + d + f + b)) / (1 + 6 * k);
	}
}

//-------------------------------------ADVECTION-------------------------------------


__global__ void advectGroundWaterRed(const float* inputQrs, const float* inputQgr, float* Qrs, float* Qgr, const float dt, const float speed)
{
	const int tX = threadIdx.x;
	if (tX % 2 == 0) return; //When 0 we return at red

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
	const float* __restrict__ defaultVelX,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neigh,
	const boundsEnv bounds,
	const float dt)
{
	// One shared for values
	__shared__ float sharedBlock[18 * 18];

	const int blockWidth = blockDim.x;
	int x = threadIdx.x + blockWidth * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + threadIdx.y * blockWidth;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		fillSharedNeigh(sharedBlock, arrayIn, defaultVal, z, bounds);
		__syncthreads();

		// We do not return, we just continue and wait for the rest of the threads. 
		if (isGroundGPU(x, y, z)) continue;
		idx = getIdx(x, y, z);

		// Downwind is when velocity is coming from the left
		bool downWind = velfieldX[idx] < 0.0f;

		float valL = downWind ? sharedBlock[x - 1] : sharedBlock[x];
		float valR = downWind ? sharedBlock[x + 1] : sharedBlock[x];
		float valC = sharedBlock[x];

		float fluxRight = advectPPMFlux(velfieldX[idx], valL, valC, valR, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velL = fillNeighbourData(neigh[idx].left, bounds, velfieldX, idx, -1, defaultVelX[y]);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velL >= 0.0f;

		valL = downWind ? sharedBlock[x - 1] : sharedBlock[x];
		valR = downWind ? sharedBlock[x + 1] : sharedBlock[x];
		valC = sharedBlock[x];

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

	const int blockWidth = blockDim.x;
	int x = threadIdx.x + blockWidth * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);
	const int idxsData = threadIdx.x + threadIdx.y * blockWidth;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		fillSharedNeigh(sharedBlock, arrayIn, defaultVal, z, bounds);
		__syncthreads();

		// We do not return, we just continue and wait for the rest of the threads. 
		if (isGroundGPU(x, y, z)) continue;

		idx = getIdx(x, y, z);
		// Downwind is when velocity is coming from the bottom
		bool downWind = velfieldY[idx] < 0.0f;

		float valD = downWind ? sharedBlock[x - blockWidth] : sharedBlock[x];
		float valU = downWind ? sharedBlock[x + blockWidth] : sharedBlock[x];
		float valC = sharedBlock[x];

		float fluxUp = advectPPMFlux(velfieldY[idx], valD, valC, valU, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velD = fillNeighbourData(neigh[idx].down, bounds, velfieldY, idx, -GRIDSIZESKYX, 0.0f);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velD >= 0.0f;

		valD = downWind ? sharedBlock[x - blockWidth] : sharedBlock[x];
		valU = downWind ? sharedBlock[x + blockWidth] : sharedBlock[x];
		valC = sharedBlock[x];

		float fluxDown = advectPPMFlux(velD, valD, valC, valU, dt);
	}
}

__global__ void advectPPMZ(const float* __restrict__ arrayIn, 
	float* __restrict__ arrayOut, 
	const float* __restrict__ defaultVal, 
	const float* __restrict__ defaultVelZ, 
	const float* __restrict__ velfieldZ, 
	const Neigh* __restrict__ neigh, 
	const boundsEnv bounds, 
	const float dt)
{
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	int idx = getIdx(x, y, z);

	// Already set forward and current to the position where they can get easily swapped in the for loop
	float forward = arrayIn[idx];
	float current = 0.0f;
	fillDataBoundCon(bounds.sides, current, arrayIn[idx], defaultVal[idx]);
	float backward = 0.0f;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		// We do not return, we just continue 
		if (isGroundGPU(x, y, z)) continue;

		backward = current;
		current = forward;
		if (neigh[idx].forward == SKY) forward = arrayIn[getIdx(x, y, z + 1)];
		else if (neigh[idx].forward == GROUND) fillDataBoundCon(bounds.ground, forward, arrayIn[idx], defaultVal[idx]);
		else fillDataBoundCon(bounds.sides, forward, arrayIn[idx], defaultVal[idx]);

		idx = getIdx(x, y, z);


		// Downwind is when velocity is coming from the left
		bool downWind = velfieldZ[idx] < 0.0f;

		float valB = downWind ? backward : current;
		float valF = downWind ? forward : current;
		float valC = current;

		float fluxForward = advectPPMFlux(velfieldZ[idx], valB, valC, valF, dt);

		// For the correct velocity, we need to do some neighbouring checks
		float velB = fillNeighbourData(neigh[idx].backward, bounds, velfieldZ, idx, -GRIDSIZESKYX * GRIDSIZESKYY, defaultVelZ[y]);

		// Now we want to check the flux from the otherside, so we turn it around
		downWind = velB >= 0.0f;

		valB = downWind ? backward : current;
		valF = downWind ? forward : current;
		valC = current;

		float fluxBackward = advectPPMFlux(velB, valB, valC, valF, dt);

		arrayOut[idx] = arrayIn[idx] - (dt / VOXELSIZE) * (fluxForward - fluxBackward);
	}
}

__global__ void advectPrecipRed(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi, 
	const float* pressuresAir, const float* groundP, const int* GHeight, const int type, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if ((tX + tY) % 2 == 0) return; //When 0 we return at red

	if (tY <= GHeight[tX]) return;

	const int idx = tX + tY * GRIDSIZESKYX;

	float fallVel = 0.0f;
	float fallVelUp = 0.0f;

	float3 fallVelocitiesPrecip{ 0.0f };
	float3 fallVelocitiesPrecipUP{ 0.0f };

	{
		const float T = float(potTemp[idx]) * powf(pressuresAir[idx] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressuresAir[idx] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecip = calculateFallingVelocityGPU(Qr[idx], Qs[idx], Qi[idx], density, type, gammaR, gammaS, gammaI);
	}
	if (neigh[idx].up == SKY)
	{
		const int iUP = idx + GRIDSIZESKYX;
		const float T = float(potTemp[iUP]) * powf(pressuresAir[iUP] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[iUP] + 1);
		const float density = pressuresAir[iUP] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecipUP = calculateFallingVelocityGPU(Qr[idx], Qs[idx], Qi[idx], density, type, gammaR, gammaS, gammaI);
	}
	switch (type)
	{
	case 0:
		fallVel = fallVelocitiesPrecip.x;
		fallVelUp = fallVelocitiesPrecipUP.x;
		break;
	case 1:
		fallVel = fallVelocitiesPrecip.y;
		fallVelUp = fallVelocitiesPrecipUP.y;
		break;
	case 2:
		fallVel = fallVelocitiesPrecip.z;
		fallVelUp = fallVelocitiesPrecipUP.z;
		break;
	default:
		break;
	}

	array[idx] -= dt * fmin((fallVel / VOXELSIZE) * array[idx], array[idx]); //Remove % of precip
	if (neigh[idx].up == SKY)
	{
		array[idx] += dt * fmin((fallVelUp / VOXELSIZE) * array[idx + GRIDSIZESKYX], array[idx + GRIDSIZESKYX]); //Grab % of precip above
	}
}

__global__ void advectPrecipBlack(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* pressuresAir, const float* groundP, const int* GHeight, const int type, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if ((tX + tY) % 2 == 1) return; //When 1 we return at black
	if (tY <= GHeight[tX]) return;


	const int idx = tX + tY * GRIDSIZESKYX;

	float fallVel = 0.0f;
	float fallVelUp = 0.0f;

	float3 fallVelocitiesPrecip{ 0.0f };
	float3 fallVelocitiesPrecipUP{ 0.0f };

	{
		const float T = float(potTemp[idx]) * powf(pressuresAir[idx] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressuresAir[idx] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecip = calculateFallingVelocityGPU(Qr[idx], Qs[idx], Qi[idx], density, type, gammaR, gammaS, gammaI);
	}
	if (neigh[idx].up == SKY)
	{
		const int iUP = idx + GRIDSIZESKYX;
		const float T = float(potTemp[iUP]) * powf(pressuresAir[iUP] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[iUP] + 1);
		const float density = pressuresAir[iUP] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecipUP = calculateFallingVelocityGPU(Qr[idx], Qs[idx], Qi[idx], density, type, gammaR, gammaS, gammaI);
	}

	switch (type)
	{
	case 0:
		fallVel = fallVelocitiesPrecip.x;
		fallVelUp = fallVelocitiesPrecipUP.x;
		break;
	case 1:
		fallVel = fallVelocitiesPrecip.y;
		fallVelUp = fallVelocitiesPrecipUP.y;
		break;
	case 2:
		fallVel = fallVelocitiesPrecip.z;
		fallVelUp = fallVelocitiesPrecipUP.z;
		break;
	default:
		break;
	}

	array[idx] -= dt * fmin((fallVel / VOXELSIZE) * array[idx], array[idx]); //Remove % of precip
	if (neigh[idx].up == SKY)
	{
		array[idx] += dt * fmin((fallVelUp / VOXELSIZE) * array[idx + GRIDSIZESKYX], array[idx + GRIDSIZESKYX]); //Grab % of precip above
	}
}

__global__ void applyPreconditionerGPU(float* output, const float* precon, const float* div, char3* A)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;
	if (isGroundGPU(tX, tY))
	{
		output[idx] = 0.0f;
		return;
	}
	output[idx] = div[idx] * precon[idx];
}

__global__ void dotProductGPU(float* result, const float* a, const float* b)
{
	//To speed up dot product, we make all blocks sum up their values and then connect them together.
	//This is faster than 1 thead doing all the work.
	__shared__ float sresult[GRIDSIZESKYX];
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (isGroundGPU(tX, tY))
	{
		sresult[tX] = 0.0f;
	}
	else
	{
		sresult[tX] = a[idx] * b[idx];
	}
	__syncthreads();

	//Basically, we grab half of the block, add all the values on the other side and repeat the process.
	for (int i = GRIDSIZESKYX / 2; i > 0; i >>= 1)
	{
		if (tX < i)
		{
			sresult[tX] += sresult[tX + i];
		}
		__syncthreads();
	}

	//Using atomicAdd(), we can safely add all block values to a singular value
	if (tX == 0)
	{
		atomicAdd(result, sresult[0]);
	}
}

__global__ void applyAGPU(float* output, const float* input, const Neigh* neigh, const char3* A)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;
	if (isGroundGPU(tX, tY))
	{
		output[idx] = 0.0f;
		return;
	}
	const float SUp = tY == GRIDSIZESKYY - 1 ? input[idx] : input[idx + GRIDSIZESKYX];
	const float SRi = tX == GRIDSIZESKYX - 1 ? input[idx] : input[idx + 1];
	const float SDo = tY == 0 ? input[idx] : input[idx - GRIDSIZESKYX];
	const float SLe = tX == 0 ? input[idx] : input[idx - 1];

	char ALeft = (neigh[idx].left == SKY) ? A[idx - 1].x : 0;
	char ADown = (neigh[idx].down == SKY) ? A[idx - GRIDSIZESKYX].y : 0;
	char3 ACur = A[idx];

	output[idx] = ACur.z * input[idx] +
					((ALeft * SLe +
						ADown * SDo +
						ACur.x * SRi +
						ACur.y * SUp)
					);
}

__global__ void calculateDivergenceGPU(float* divergence, const Neigh* neigh, const float* velX, const float* velY, const float* dens, const float* oldDens, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (isGroundGPU(tX, tY))
	{
		divergence[idx] = 0.0f;
		return;
	}
	

	//If Right or Left at the outside cell. We use the default values.
	//If At Ceiling, we don't have to do anything: y is already set to 0, and ceiling is free-slip
	//If at Ground, we set velocity to 0, creating divergence, making sure this is no-slip.

	const float densC = dens[idx];
	const float Ucurr = (neigh[idx].right == OUTSIDE && idx != 0) ? defaultVel[tY] : (neigh[idx].right == GROUND ? 0.0f : velX[idx]);
	const float Umin1 = (neigh[idx].left == OUTSIDE || idx == 0) ? defaultVel[tY] : (neigh[idx].left == GROUND ? 0.0f : velX[idx - 1]);
	const float Vcurr = velY[idx];
	const float Vmin1 = (neigh[idx].down == SKY && idx - GRIDSIZESKYX >= 0) ? velY[idx - GRIDSIZESKYX] : 0.0f;

	const float DR = neigh[idx].right == SKY ? 0.5f * (densC + dens[idx + 1]) : densC;
	const float DL = neigh[idx].left == SKY ? 0.5f * (densC + dens[idx - 1]) : densC;
	const float DU = neigh[idx].up == SKY ? 0.5f * (densC + dens[idx + GRIDSIZESKYX]) : densC;
	const float DD = neigh[idx].down == SKY ? 0.5f * (densC + dens[idx - GRIDSIZESKYX]) : densC;

	const float massFluxDiv = ((DR * Ucurr - DL * Umin1) + (DU * Vcurr - DD * Vmin1));

	//Change in density per second (which is why we use dt)
	const float densityChange = (dens[idx] - oldDens[idx]) / dt;

	//Using divergence minus the change in compressibility
	//Dividing again by dt to get kg/m3*s2 instead of only s
	divergence[idx] = (massFluxDiv - densityChange) ;
}

__global__ void applyPresProjGPU(const float* pressure, const Neigh* neigh, float* velX, float* velY, const float* density,const float* pressureEnv, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (isGroundGPU(tX, tY)) return;

	//If at the right or upper cell, we don't add any value
	const float NxPresProj = neigh[idx].right != SKY ? 0.0f : pressure[idx + 1] - pressure[idx];
	const float NyPresProj = neigh[idx].up != SKY ? 0.0f : pressure[idx + GRIDSIZESKYX] - pressure[idx];


	//Pressure at face
	const float NxDens = neigh[idx].right != SKY ? density[idx] : (2.0f / (1.0f / density[idx] + 1.0f / density[idx + 1]));
	const float NyDens = neigh[idx].up != SKY ? density[idx] : (2.0f / (1.0f / density[idx] + 1.0f / density[idx + GRIDSIZESKYX]));

	//Using dt to match dt used in divergence calculation
	//Dividing by density at cell faces gives for pressure effects
	velX[idx] += NxPresProj / NxDens;
	velY[idx] += NyPresProj / NyDens;
}

__global__ void getMaxDivergence(float* output, const float* div)
{
	//To speed up dot product, we make all blocks sum up their values and then connect them together.
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

__global__ void updatePandDiv(float* S1, float* S2, float* pressure, float* divergence, const float* s, const float* z)
{
	__shared__ float sresult;
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (tX == 0)
	{
		sresult = *S1 / *S2;
	}

	__syncthreads();

	//Adding up pressure value and reducing residual value
	if (!isGroundGPU(tX, tY))
	{
		pressure[idx] += sresult * s[idx];
		divergence[idx] -= sresult * z[idx];
	}
	if (idx == 0)
	{
		*S2 = sresult;
	}
}

__global__ void endIteration(float* S1, float* S2, float* s, const float* z)
{
	__shared__ float sresult;
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (tX == 0)
	{
		sresult = *S2 / *S1;
	}
	__syncthreads();

	if (!isGroundGPU(tX, tY))
	{
		//Setting search vector s
		s[idx] = z[idx] + sresult * s[idx];
	}

	if (idx == 0)
	{
		*S1 = *S2;
	}
}

__global__ void updatePressure(float* envPressure, const float* presProj)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	envPressure[idx] -= presProj[idx] / 100.0f; //Pa to Pha

}

//-------------------------------------OTHER-------------------------------------

__global__ void calculateCloudCover(float* output, const float* Qc, const float* Qw, const int* GHeight)
{
	const int tX = threadIdx.x;

	float totalCloudContent = 0.0f;
	const float qfull = 1.2f; // a threshold value where all incoming radiation is reflected by cloud matter: http://meto.umd.edu/~zli/PDF_papers/Li%20Nature%20Article.pdf
	for (int y = GHeight[tX] + 1; y < GRIDSIZESKYY; y++) totalCloudContent += (Qc[tX + y * GRIDSIZESKYX] + Qw[tX + y * GRIDSIZESKYX]) * VOXELSIZE;
	output[tX] = fmin(totalCloudContent / qfull, 1.0f);
}

__global__ void calculateGroundTemp(float* groundT, const float dtSpeed, const float irridiance, const float* LC)
{
	const int tX = threadIdx.x;

	const float absorbedRadiationAlbedo = 0.25f;  //How much light is reflected back? 0 = absorbes all, 1 = reflects all
	const float groundThickness = 1.0f; //Just used 1 meter
	const float densityGround = 1500.0f;
	const double T4 = groundT[tX] * groundT[tX] * groundT[tX] * groundT[tX];

	groundT[tX] += dtSpeed * ((1 - LC[tX]) * (((1 - absorbedRadiationAlbedo) * irridiance - ConstantsGPU::ge * ConstantsGPU::oo * T4) / (groundThickness * densityGround * ConstantsGPU::Cpds)));
}

__device__ float calculateLayerAverage(const float* layer, const float maxDistance, const int lY, const bool temp)
{
	const int oX = threadIdx.x; //Original X
	const int oY = blockIdx.x; //Original Y
	// lY = Layer Y

	const int distXR = oX + int(ceilf(maxDistance));
	const int distXL = oX - int(ceilf(maxDistance));
	const int maxRight = distXR > GRIDSIZESKYX ? GRIDSIZESKYX : distXR;
	const int minLeft = distXL < 0 ? -1 : distXL;
	const float MDistanceSqr = 1 / (maxDistance * maxDistance);
	const int distOYY = (oY - lY) * (oY - lY);

	float amount = 0.0f;
	float average = 0.0f;

	for (int x = oX + 1; x < maxRight; x++)
	{
		if (x >= GRIDSIZESKYX || isGroundGPU(x, lY)) break;

		const int distOXX = oX - x;
		const float distance = float(distOXX * distOXX + distOYY);
		float cAmount = -(distance * MDistanceSqr - 1);
		if (cAmount > 1.0f || cAmount <= 0.0f) continue;
		//Smoothing
		cAmount = cAmount * cAmount * cAmount * cAmount * cAmount;
		amount = amount + cAmount;
		average += layer[x] * cAmount;
	}
	for (int x = oX - 1; x > minLeft; x--)
	{
		if (x < 0 || isGroundGPU(x, lY)) break;

		const int distOXX = oX - x;
		const float distance = float(distOXX * distOXX + distOYY);
		float cAmount = -(distance * MDistanceSqr - 1);
		if (cAmount > 1.0f || cAmount <= 0.0f) continue;
		//Smoothing
		cAmount = cAmount * cAmount * cAmount * cAmount * cAmount;
		amount = amount + cAmount;
		average += layer[x] * cAmount;
	}
	if (amount == 0.0f) return layer[oX];

	return (average / amount);
}

__global__ void buoyancyGPU(float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* pressures, const float* groundP, float* buoyancyStor, const float maxDistance, const float dt)
{
	//Shared data in one block, for layer up down and current to calculate average faster
	//First use it for Qv, then potTemp
	__shared__ float sharedLayer[GRIDSIZESKYX];
	__shared__ float sharedLayerUp[GRIDSIZESKYX];
	__shared__ float sharedLayerDown[GRIDSIZESKYX];

	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;
	bool up = neigh[idx].up == SKY;
	bool down = neigh[idx].down == SKY;

	//Set current, up and down layer to be Qv if there is up or down
	sharedLayer[tX] = Qv[idx];
	sharedLayerUp[tX] = up ? Qv[idx + GRIDSIZESKYX] : -1.0f;
	sharedLayerDown[tX] = down ? Qv[idx - GRIDSIZESKYX] : -1.0f;

	//Wait for every thread to be done (for sure).
	__syncthreads();


	//Vapor environment and Vapor Parcel
	float Qenv = 0.0f, QenvUp = 0.0f, QenvDown = 0.0f;
	float QP = 0.0f, QPUp = 0.0f, QPDown = 0.0f;
	int idxUp = up ? idx + GRIDSIZESKYX : idx;
	int idxDown = down ? idx - GRIDSIZESKYX : idx;

	const float TDown = static_cast<float>(potTemp[idxDown]) * powf(pressures[idxDown] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
	const float T = static_cast<float>(potTemp[idx]) * powf(pressures[idx] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
	const float TUp = static_cast<float>(potTemp[idxUp]) * powf(pressures[idxUp] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);

	// ------------ QV ------------
	if (down)
	{
		QenvDown = calculateLayerAverage(sharedLayerDown, maxDistance, tY - 1, false);
		QPDown = Qv[idx]; //Amount of vapor stays the same
	}
	Qenv = calculateLayerAverage(sharedLayer, maxDistance, tY, false);
	QP = Qv[idx];
	if (up)
	{
		QenvUp = calculateLayerAverage(sharedLayerUp, maxDistance, tY + 1, false);
		QPUp = Qv[idx];
	}


	__syncthreads();

	// ------------ Temp ------------

	// Re-use layers
	sharedLayer[tX] = potTemp[idx];
	sharedLayerUp[tX] = up ? potTemp[idx + GRIDSIZESKYX] : -1.0f;
	sharedLayerDown[tX] = down ? potTemp[idx - GRIDSIZESKYX] : -1.0f;

	//Wait for every thread to be done (for sure).
	__syncthreads();

	//Now check if we are on the edges, if so, we leave
	//if (isGroundGPU(tX, tY) || neigh[idx].left == OUTSIDE || neigh[idx].right == OUTSIDE || neigh[idx].up == OUTSIDE || neigh[idx].down == OUTSIDE)
	//{
	//	buoyancyStor[idx] = 0.0f;
	//	return;
	//}

	// Temp for up and down based on adiabatics
	float Tadiab = sharedLayer[tX], TadiabUp = sharedLayer[tX], TadiabDown = sharedLayer[tX];
	// Environment Temp
	float Tenv = sharedLayer[tX], TenvUp = sharedLayer[tX], TenvDown = sharedLayer[tX];

	//First we calculate the parcel temps for each different heights
	//This is different when the air is saturated
	if (down)
	{
		//If going downwards, we still check if this downwards parcel would be saturated
		if ((T < 0.0f && Qv[idx] >= wiGPU((TDown - 273.15f), pressures[idxDown])) || Qv[idx] >= wsGPU((TDown - 273.15f), pressures[idxDown]))
		{
			//Need real temperature to calculate moist adiabatic
			TadiabDown = T - MLRGPU(T - 273.15f, pressures[idx]) * (pressures[idx] - pressures[idxDown]);
			//Convert back to potTemp
			TadiabDown = TadiabDown * powf(groundP[tX] / pressures[idxDown], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		}
		TenvDown = calculateLayerAverage(sharedLayerDown, maxDistance, tY - 1, true);
	}
	if (up)
	{
		//Vapor amount would be greater than downwards temp could hold, so we use moist adiabatic
		if ((T < 0.0f && Qv[idx] >= wiGPU((TUp - 273.15f), pressures[idxUp])) || Qv[idx] >= wsGPU((TUp - 273.15f), pressures[idxUp]))
		{
			//Need real temperature to calculate moist adiabatic
			TadiabUp = MLRGPU(T - 273.15f, pressures[idx]) * (pressures[idxUp] - pressures[idx]) + T;
			//Convert back to potTemp
			TadiabUp = TadiabUp * powf(groundP[tX] / pressures[idxUp], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		}
		TenvUp = calculateLayerAverage(sharedLayerUp, maxDistance, tY + 1, true);
	}

	Tenv = calculateLayerAverage(sharedLayer, maxDistance, tY, true);


	// ------------ Buoyancy ------------

	// Seperate buoyancies
	float B = 0.0f, BUp = 0.0f, BDown = 0.0f;

	const float totalQ = 0.0f;// Qr[idx] + Qs[idx] + Qi[idx];
	if (down)
	{
		//Temp parcel and Temp environment
		const float VTP = TadiabDown * (1.0f + 0.608f * QPDown);
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
		const float VTP = TadiabUp * (1.0f + 0.608f * QPUp);
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

__global__ void addHeatGPU(const float* _Qv, float* potTemp, float* condens, float* depos, float* freeze)
{
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

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

__global__ void computeNeighbourGPU(Neigh* Neigh)
{
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;

	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdxGPU(x, y, z);

		Neigh[idx].left =  (x == 0) ? OUTSIDE : (isGroundGPU(x - 1, y, z) ? GROUND : SKY);
		Neigh[idx].right = (x == GRIDSIZESKYX - 1) ? OUTSIDE : (isGroundGPU(x + 1, y, z) ? GROUND : SKY);
		Neigh[idx].down =  (y == 0) ? GROUND : (isGroundGPU(x, y - 1, z) ? GROUND : SKY); // Down is always ground (at y == 0)
		Neigh[idx].up =    (y == GRIDSIZESKYY - 1) ? OUTSIDE : (isGroundGPU(x, y + 1, z) ? GROUND : SKY);
		Neigh[idx].backward = (z == 0) ? OUTSIDE : (isGroundGPU(x, y, z - 1) ? GROUND : SKY);
		Neigh[idx].forward = (z == GRIDSIZESKYZ - 1) ? OUTSIDE : (isGroundGPU(x, y, z + 1) ? GROUND : SKY);
	}
}



__global__ void initAMatrix(char3* A, const Neigh* neigh, const float* density)
{
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;
	const float densC = density[idx];

	//Fill matrix A	
	A[idx].x = 0;
	A[idx].y = 0;
	A[idx].z = 0;

	//Right and Left side are neumann, thus we include it into the matrix
	//Ceiling, and any ground will not be counted

	if (isGroundGPU(tX, tY))
	{
		return;
	}

	float densR = neigh[idx].right == SKY ? density[idx + 1] : (neigh[idx].right == GROUND ? 0.0f : densC);
	float densL = neigh[idx].left  == SKY ? density[idx - 1] : (neigh[idx].left == GROUND ? 0.0f : densC);
	float densU = neigh[idx].up    == SKY ? density[idx + GRIDSIZESKYX] : (neigh[idx].up == GROUND ? 0.0f : 0.0f);
	float densD = neigh[idx].down  == SKY ? density[idx - GRIDSIZESKYX] : (neigh[idx].down == GROUND ? 0.0f : densC);

	//Calculate harmonic mean: https://en.wikipedia.org/wiki/Harmonic_mean
	//Also normalizing (1.225 / answer) to make sure values don't reach below 1.0f (if so, it would crash)
	//Using max for if density > 1.225, and the -  is to make values negative.
	densR = densR == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / densC + 1.0f / densR)));
	densL = densL == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / densC + 1.0f / densL)));
	densU = densU == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / densC + 1.0f / densU)));
	densD = densD == 0.0f ? 0.0f : -fmaxf(1.0f, 1.225f / (2.0f / (1.0f / densC + 1.0f / densD)));

	A[idx].x = densR;
	A[idx].y = densU;
	//Using - because calculated density is already set to be negative, so this makes positive
	if (A[idx].y) A[idx].z -= densU;
	if (A[idx].x) A[idx].z -= densR;
	if (neigh[idx].down == SKY) A[idx].z -= densD;
	if (neigh[idx].left != GROUND) A[idx].z -= densL;
}

__global__ void initPrecon(float* precon, const char3* A)
{
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

	precon[idx] = 1.0f;
	if (isGroundGPU(tX, tY))
	{
		return;
	}
	precon[idx] = A[idx].z == 0.0f ? 1.0f : 1.0f / A[idx].z;

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
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = 0;
	
	for (z = 0; z < GRIDSIZESKYZ; z++)
	{
		int idx = getIdx(x, y, z);

		const float T = float(potTemp[idx]) * glm::pow(pressures[idx] / groundP[x + z * GRIDSIZESKYX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressures[idx] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa

		densityAir[idx] = density;
	}
}

__global__ void calculateNewPressure(float* pressureEnv, const float* densityAir, const float* potTemp, const float* Qv, const float* GPressure)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

	//Yes, we use pressure to first calculate T and then calculate pressure, it is double sided but how else?
	const float T = float(potTemp[idx]) * glm::pow(pressureEnv[idx] / GPressure[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
	const float Tv = T * (0.608f * Qv[idx] + 1);
	pressureEnv[idx] = densityAir[idx] * ConstantsGPU::Rsd * Tv / 100.0f;
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


__global__ void resetVelPressProj(const Neigh* neigh, float* velX, float* velY)
{
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

	if (isGroundGPU(tX, tY)) return;

	if (neigh[idx].up != SKY)
	{
		velY[idx] = 0.0f;
	}
	if (neigh[idx].right != SKY)
	{
		velX[idx] = defaultVel[tY];
	}
}

__device__ bool isGroundLevel()
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	return y == (GHeight[x] + 1);
}

//Usage of threads
__device__ bool isGroundGPU()
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	return y <= GHeight[x];
}

__device__ bool isGroundGPU(const int x, const int y, const int z)
{
	return y <= GHeight[x + z * GRIDSIZESKYX];
}

__global__ void setToDefault(float* array, const float* defaultValue)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * blockDim.x;

	array[idx] = defaultValue[tY];
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
		//Reset offset in case y is also offset
		sharedIdx -= extraIdxRight + extraIdxLeft;
	}
	if (y != extraY)
	{
		//Offset sharedIdx, and based on the type we set our data
		sharedIdx += extraIdxUp + extraIdxDown;
		//CustomDataVal is not updated with new offset, but thats okay
		switch (verType)
		{
		case SKY:
			sharedData[sharedIdx] = data[getIdx(extraX, extraY, z)];
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
	}
}

__device__ __forceinline__ float fillNeighbourData(envType neighbourType, boundsEnv condition, const float* data, const int idx, const int offset, const float customData, bool up)
{
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
