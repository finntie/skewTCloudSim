#include "kernelSky.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "environment.h"
#include "environment.cuh"
#include "meteoconstants.cuh"
#include "meteoformulas.cuh"
#include "editor.h"

__constant__ int GHeight[GRIDSIZEGROUND];
__constant__ float defaultVel[GRIDSIZESKYY];

__constant__ float gammaR;
__constant__ float gammaS;
__constant__ float gammaI;

void initKernelSky(const int* _GHeight, const float* _defaultVel)
{
	//Set constant data for easier access
	cudaMemcpyToSymbol(GHeight, _GHeight, GRIDSIZEGROUND * sizeof(int));
	cudaMemcpyToSymbol(defaultVel, _defaultVel, GRIDSIZESKYY * sizeof(float));

	const float b = 0.8f;
	const float d = 0.25f;
	float GammaR = tgammaf(4.0f + b);
	float GammaS = tgammaf(4.0f + d);
	float GammaI = GammaS;
	cudaMemcpyToSymbol(gammaR, &GammaR, sizeof(float));
	cudaMemcpyToSymbol(gammaS, &GammaS, sizeof(float));
	cudaMemcpyToSymbol(gammaI, &GammaI, sizeof(float));
}

__global__ void diffuseRed(const Neigh* neigh, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal, 
	const float* input, float* output, const float k, const int type)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;

	if (isGroundGPU(tX, tY)) return;
	if ((tX + tY) % 2 == 1) return;

	const int idx = tX + tY * GRIDSIZESKYX;
	const int idxGL = tX == 0 ? tX : tX - 1;
	const int idxGR = tX + 1;
	const int idxL = tX == 0 ? idx : idx - 1;
	const int idxR = idx + 1;
	const int idxGD = tX;
	float l = 0.0f;
	float r = 0.0f;
	float d = 0.0f;
	float u = 0.0f;
	switch (type)
	{
	case 0: //Temp
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] :   (neigh[idx].left == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGL] - 273.15f, pressures[tY], groundP[idxGL]) + 273.15f) : (neigh[idx].left == GROUND  ?input[idx] : input[idxL]));
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : (neigh[idx].right == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGR] - 273.15f, pressures[tY], groundP[idxGR]) + 273.15f) : (neigh[idx].right == GROUND ?input[idx] : input[idxR]));
		d = neigh[idx].down == GROUND ? potentialTempGPU(groundT[idxGD] - 273.15f, pressures[tY], groundP[idxGD]) + 273.15f : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 1: case 4: //Vapor or default
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? input[idx] : input[idxL];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? input[idx] : input[idxR];
		d = neigh[idx].down == GROUND ? input[idx] : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 2: //Vel X
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? 0.0f : input[idxL];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? 0.0f : input[idxR];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; //No-Slip
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	case 3: //Vel Y
		l = neigh[idx].left == OUTSIDE ? input[idx] : neigh[idx].left == GROUND ? 0.0f : input[idxL];
		r = neigh[idx].right == OUTSIDE ? input[idx] : neigh[idx].right == GROUND ? 0.0f : input[idxR];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; // No-Slip
		u = neigh[idx].up == OUTSIDE ? 0.0f : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	}

	output[idx] = (input[idx] + k * (l + r + u + d)) / (1 + 4 * k);
}

__global__ void diffuseBlack(const Neigh* neigh, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal, 
	const float* input, float* output, const float k, const int type)
{

	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if (isGroundGPU(tX, tY)) return;
	if ((tX + tY) % 2 == 0) return;

	const int idx = tX + tY * GRIDSIZESKYX;
	const int idxGL = tX - 1;
	const int idxGR = tX == GRIDSIZEGROUND - 1 ? tX : tX + 1;
	const int idxL = idx - 1;
	const int idxR = tX == GRIDSIZESKYX - 1 ? tX : idx + 1;
	float l = 0.0f;
	float r = 0.0f;
	float d = 0.0f;
	float u = 0.0f;
	switch (type)
	{
	case 0: //Temp
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : (neigh[idx].left == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGL] - 273.15f, pressures[tY], groundP[idxGL]) + 273.15f) : (neigh[idx].left == GROUND ? input[idx] : input[idxL]));
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : (neigh[idx].right == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGR] - 273.15f, pressures[tY], groundP[idxGR]) + 273.15f) : (neigh[idx].right == GROUND ? input[idx] : input[idxR]));
		d = neigh[idx].down == GROUND ? potentialTempGPU(groundT[tX] - 273.25f, pressures[tY], groundP[tX]) + 273.15f : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 1: case 4: //Vapor or default
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? input[idx] : input[idxL];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? input[idx] : input[idxR];
		d = neigh[idx].down == GROUND ? input[idx] : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 2: //Vel X
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? 0.0f : input[idxL];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? 0.0f : input[idxR];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; //No-Slip
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	case 3: //Vel Y
		l = neigh[idx].left == OUTSIDE ? input[idx] : neigh[idx].left == GROUND ? 0.0f : input[idxL];
		r = neigh[idx].right == OUTSIDE ? input[idx] : neigh[idx].right == GROUND ? 0.0f : input[idxR];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; // No-Slip
		u = neigh[idx].up == OUTSIDE ? 0.0f : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	}

	output[idx] = (input[idx] + k * (l + r + u + d)) / (1 + 4 * k);
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

__global__ void setTempsAtGroundGPU(float* potTemps, const float* groundTemps, const float* pressures, const float* groundPressures, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = GHeight[tX] + 1;
	const int idx = tX + tY * GRIDSIZESKYX;
	
	if (tY >= GRIDSIZESKYY - 1) return;

	const float T = potentialTempGPU(groundTemps[tX] - 273.15f, pressures[tY], groundPressures[tX]) + 273.15f;
	const float dif2 = potTemps[idx] - T;
	potTemps[idx] -= dif2 * fminf(1.0f, dt);
}

__device__ float advectPPMFlux(const float* array, const float defaultVal, const float velfield, const Neigh neighbour, const Neigh downWindNeigh,
	const float dt, const int tX, const bool x, const bool isRight)
{
	//Inputted array has size of 1 block

	//Dir is same for x and y due to block being 1D array
	const int dir = isRight ? 1 : -1;

	float veli = velfield;

	//Calculate C (Courant number)
	float C = veli * dt / VOXELSIZE;
	//if (fabsf(C) > 1.0f) printf("HOW IS THIS HIGHER THAN 1.0??, %f %f, %f\n", veli, dt, C);
	//We limit, this is not good, but we already do substeps, so just in case to not get an error:
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

		//if (downWindNeigh.down != SKY) printf("Array[%i]: %f\n", i, array[i]);
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

__global__ void advectPPMX(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neighbour,
	const float dt)
{

	//if (isGroundGPU()) return;

	__shared__ float sArray[GRIDSIZESKYX];
	__shared__ float sVelX[GRIDSIZESKYX];

	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tY * GRIDSIZESKYX + tX;

	Neigh neigh = neighbour[idx];
	float defVal = 0.0f;
	if (defaultVal)
	{
		defVal = defaultVal[tY];
	}


	if (isGroundGPU(tX, tY))
	{
		sArray[tX] = defVal;
		sVelX[tX] = velfieldX[idx];
		//printf("sArray[%i] = %f\n", tX, sArray[tX]);
		//printf("sVelX[%i] = %f\n", tX, sVelX[tX]);
		return;
	}
	else
	{
		sArray[tX] = arrayIn[idx];
		sVelX[tX] = velfieldX[idx];
	}
	if (idx >= GRIDSIZESKY) return;

	__syncthreads();



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

__global__ void advectPPMY(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neighbour,
	const float dt)
{
	__shared__ float sArray[GRIDSIZESKYY];
	__shared__ float sVelY[GRIDSIZESKYY];

	int tY = threadIdx.x;//Real Y
	int idxY = blockIdx.x; //Real X
	int idx = tY * GRIDSIZESKYX + idxY;

	Neigh neigh = neighbour[idx];
	float defVal = 0.0f;
	if (defaultVal)
	{
		defVal = defaultVal[tY];
	}

	if (isGroundGPU(idxY, tY))
	{
		sArray[tY] = defVal;
		sVelY[tY] = velfieldY[idx];
		//printf("(%i, %i) %i sArray[%i] = %f\n", idxY, tY, idx, tY, sArray[tY]);
		//printf("sVelY[%i] = %f\n", tY, sVelY[tY]);
		return;
	}
	else
	{
		sArray[tY] = arrayIn[idx];
		sVelY[tY] = velfieldY[idx];
	}
	if (idx >= GRIDSIZESKY) return;

	__syncthreads();



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

__global__ void advectPrecipRed(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi, 
	const float* pressures, const float* groundP, const int* GHeight, const int type, const float dt)
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
		const float T = float(potTemp[idx]) * powf(pressures[tY] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressures[tY] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecip = calculateFallingVelocityGPU(Qr[idx], Qs[idx], Qi[idx], density, type, gammaR, gammaS, gammaI);
	}
	if (neigh[idx].up == SKY)
	{
		const int iUP = idx + GRIDSIZESKYX;
		const int yUP = tY + 1;
		const float T = float(potTemp[iUP]) * powf(pressures[yUP] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[iUP] + 1);
		const float density = pressures[yUP] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
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
	const float* pressures, const float* groundP, const int* GHeight, const int type, const float dt)
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
		const float T = float(potTemp[idx]) * powf(pressures[tY] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressures[tY] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecip = calculateFallingVelocityGPU(Qr[idx], Qs[idx], Qi[idx], density, type, gammaR, gammaS, gammaI);
	}
	if (neigh[idx].up == SKY)
	{
		const int iUP = idx + GRIDSIZESKYX;
		const int yUP = tY + 1;
		const float T = float(potTemp[iUP]) * powf(pressures[yUP] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[iUP] + 1);
		const float density = pressures[yUP] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
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
	float ADiag = 1.0f / A[idx].z;

	output[idx] = div[idx] * precon[idx] / ADiag;
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

__global__ void calculateDivergenceGPU(float* divergence, const Neigh* neigh, const float* velX, const float* velY)
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

	const float Ucurr = (neigh[idx].right == OUTSIDE && idx != 0) ? defaultVel[tY] : (neigh[idx].right == GROUND ? 0.0f : velX[idx]);
	const float Umin1 = (neigh[idx].left == OUTSIDE || idx == 0) ? defaultVel[tY] : (neigh[idx].left == GROUND ? 0.0f : velX[idx - 1]);
	const float Vcurr = velY[idx];
	const float Vmin1 = (neigh[idx].down == SKY && idx - GRIDSIZESKYX >= 0) ? velY[idx - GRIDSIZESKYX] : 0.0f;

	divergence[idx] = ((Ucurr - Umin1) + (Vcurr - Vmin1));
}

__global__ void applyPresProjGPU(const float* pressure, const Neigh* neigh, float* velX, float* velY)
{
	if (isGroundGPU()) return;

	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	//If at the right or upper cell, we don't add any value
	const float NxPresProj = (neigh[idx].right != SKY) ? pressure[idx] : pressure[idx + 1];
	const float NyPresProj = neigh[idx].up != SKY ? pressure[idx] : pressure[idx + GRIDSIZESKYX];

	const float scale = 1.0f;

	velX[idx] += scale * (NxPresProj - pressure[idx]);
	velY[idx] += scale * (NyPresProj - pressure[idx]);
}

__global__ void getMaxDivergence(float* output, const float* div)
{
	//To speed up dot product, we make all blocks sum up their values and then connect them together.
		//This is faster than 1 thead doing all the work.
	__shared__ float sresult[GRIDSIZESKYX];
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	float maxVal = 0.0f;
	if (!isGroundGPU(tX, tY))
	{
		maxVal = fabsf(div[idx]);
	}
	sresult[tX] = maxVal;
	__syncthreads();

	//Basically, we grab half of the block, add all the values on the other side and repeat the process.
	for (int i = GRIDSIZESKYX / 2; i > 0; i >>= 1)
	{
		if (tX < i)
		{
			sresult[tX] = fmaxf(sresult[tX] ,sresult[tX + i]);
		}
		__syncthreads();
	}

	//Using atomicAdd(), we can safely add all block values to a singular value
	if (tX == 0)
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

__device__ float calculateLayerAverage(const float* layer, const float* pressures, const float* groundP, const float maxDistance, const int lY, const bool temp)
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

	//TODO: when difference in ground pressure, remove optimization:
	float potTempMultiplier = temp ? powf((pressures[lY] / groundP[0]), ConstantsGPU::Rsd / ConstantsGPU::Cpd) : 1.0f;

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
		average += layer[x] * potTempMultiplier * cAmount;
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
		average += layer[x] * potTempMultiplier * cAmount;
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
	int tYCheck = up ? tY + 1 : tY;

	const float T = static_cast<float>(potTemp[idx]) * powf(pressures[tY] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
	const float T2 = static_cast<float>(potTemp[tX + tYCheck * GRIDSIZESKYX]) * powf(pressures[tYCheck] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);

	// ------------ QV ------------
	if (down)
	{
		QenvDown = calculateLayerAverage(sharedLayerDown, pressures, groundP, maxDistance, tY - 1, false);
		QPDown = Qv[idx]; //Mixing ratio stays the same?
	}
	Qenv = calculateLayerAverage(sharedLayer, pressures, groundP, maxDistance, tY, false);
	QP = Qv[idx];
	if (up)
	{
		QenvUp = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY + 1, false);
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
	if (isGroundGPU(tX, tY) || neigh[idx].left == OUTSIDE || neigh[idx].right == OUTSIDE || neigh[idx].up == OUTSIDE || neigh[idx].down == OUTSIDE)
	{
		buoyancyStor[idx] = 0.0f;
		return;
	}

	// Temp for up and down based on adiabatics
	float Tadiab = T, TadiabUp = T, TadiabDown = T;
	// Environment Temp
	float Tenv = T, TenvUp = T, TenvDown = T;

	//First we calculate the parcel temps for each different heights
	if (Qv[idx] >= wsGPU((T2 - 273.15f), pressures[tY + 1]))
	{
		//Air is saturated, thus use moist adiabatic
		if (down)
		{	
			//TODO: wrong and not very good if P change > 1
			TadiabDown = T - MLRGPU(T - 273.15f, pressures[tY]) * (pressures[tY] - pressures[tY - 1]);
			TenvDown = calculateLayerAverage(sharedLayerDown, pressures, groundP, maxDistance, tY - 1, true);
			//printf("(%i, %i) AV: %f, P: %f\n", tX, tY, TenvDown, TadiabDown);
		}
		if (up)
		{
			TadiabUp = MLRGPU(T - 273.15f, pressures[tY]) * (pressures[tY + 1] - pressures[tY]) + T;
			TenvUp = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY + 1, true);
		}
	}
	else
	{
		//Air is dry, thus we use dry adiabatic
		if (down)
		{
			TadiabDown = T * powf(pressures[tY - 1] / pressures[tY], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			TenvDown = calculateLayerAverage(sharedLayerDown, pressures, groundP, maxDistance, tY - 1, true);
		}
		if (up)
		{
			TadiabUp = T * powf(pressures[tY + 1] / pressures[tY], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			TenvUp = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY + 1, true);
		}
	}
	Tenv = calculateLayerAverage(sharedLayer, pressures, groundP, maxDistance, tY, true);


	// ------------ Buoyancy ------------

	// Seperate buoyancies
	float B = 0.0f, BUp = 0.0f, BDown = 0.0f;

	const float totalQ = Qr[idx] + Qs[idx] + Qi[idx];

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
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

	Neigh[idx].left = (tX == 0) ? OUTSIDE : (isGroundGPU(tX - 1, tY) ? GROUND : SKY);
	Neigh[idx].right = (tX == GRIDSIZESKYX - 1) ? OUTSIDE : (isGroundGPU(tX + 1, tY) ? GROUND : SKY);
	Neigh[idx].down = (tY == 0) ? GROUND : (isGroundGPU(tX, tY - 1) ? GROUND : SKY); // Down is always ground (at y == 0)
	Neigh[idx].up = (tY == GRIDSIZESKYY - 1) ? OUTSIDE : (isGroundGPU(tX, tY + 1) ? GROUND : SKY);
}

__global__ void initAandPrecon(char3* A, float* precon, const Neigh* neigh)
{
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

	//Fill matrix A	
	A[idx].x = 0;
	A[idx].y = 0;
	A[idx].x = 0;

	//Right and Left side are neumann, thus we include it into the matrix
	//Ceiling, and any ground will not be counted

	precon[idx] = 0.0f;
	if (isGroundGPU(tX, tY))
	{
		return;
	}

	A[idx].x = neigh[idx].right != GROUND ? -1 : 0;
	A[idx].y = neigh[idx].up == SKY ? -1 : 0;
	A[idx].z = 0;

	if (A[idx].x) A[idx].z++;
	if (A[idx].y) A[idx].z++;
	if (neigh[idx].left != GROUND) A[idx].z++;
	if (neigh[idx].down == SKY) A[idx].z++;


	const float Tune = 0.97f;

	//TODO: for 3D, look at formula 4.34	
	int Aminxx = tX == 0 ? 0 : A[idx - 1].x; //Minus X looking at x
	int Aminxy = tX == 0 ? 0 : A[idx - 1].y; //Minus X looking at y
	int Aminyy = tY == 0 ? 0 : A[idx - GRIDSIZESKYX].y; //Minus Y looking at y
	int Aminyx = tY == 0 ? 0 : A[idx - GRIDSIZESKYX].x; //Minus Y looking at x

	//This does not have effect due to precon calculated in parrallel. 
	const float Preconi = tX == 0 ? precon[idx] : precon[idx - 1];
	const float Preconj = tY == 0 ? precon[idx] : precon[idx - GRIDSIZESKYX];

	const float e = A[idx].z
		- (Aminxx * Preconi) * (Aminxx * Preconi)
		- (Aminyy * Preconj) * (Aminyy * Preconj)
		- Tune * (
			Aminxx * Aminxy * (Preconi * Preconi) +
			Aminyy * Aminyx * (Preconj * Preconj));
	precon[idx] = (1 / sqrtf(e + 1e-30f)); //Prevent division by 0 using small number;
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
	float* Qv, float* Qw, float* Qc, float* Qr, float* Qs, float* Qi, float* potTemp, float* velX, float* velY)
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

__device__ bool isGroundGPU(const int x, const int y)
{
	return y <= GHeight[x];
}

__global__ void setToDefault(float* array, const float* defaultValue)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * blockDim.x;

	array[idx] = defaultValue[tY];
}
