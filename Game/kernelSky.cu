#include "kernelSky.cuh"

#include <CUDA/include/cuda_runtime.h>
#include <CUDA/include/cuda.h>
#include <CUDA/cmath>

//Game includes
#include "environment.h"
#include "meteoconstants.cuh"
#include "meteoformulas.cuh"

__constant__ float GHeight[GRIDSIZEGROUND];
__constant__ float defaultVel[GRIDSIZESKYY];



void initKernelSky(const int* _GHeight, const float* _defaultVel)
{
	//Set constant data for easier access
	cudaMemcpyToSymbol(GHeight, _GHeight, GRIDSIZEGROUND * sizeof(float));
	cudaMemcpyToSymbol(defaultVel, _defaultVel, GRIDSIZESKYY * sizeof(float));
}

__global__ void diffuseRed(const Neigh* neigh, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal, 
	const float* input, float* output, const float k, const int type)
{
	if (isGroundGPU()) return;

	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if ((tX + tY) % 2 == 1) return;

	const int idx = tX + tY * GRIDSIZESKYX;
	const int idxGL = tX - 1;
	const int idxGR = tX + 1;
	const int idxGD = tX;
	float l = 0.0f;
	float r = 0.0f;
	float d = 0.0f;
	float u = 0.0f;
	switch (type)
	{
	case 0: //Temp
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGL] - 273.25f, pressures[tY], groundP[idxGL]) + 273.15f) : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGR] - 273.25f, pressures[tY], groundP[idxGR]) + 273.15f) : input[idx + 1];
		d = neigh[idx].down == GROUND ? potentialTempGPU(groundT[idxGD] - 273.25f, pressures[tY], groundP[idxGD]) + 273.15f : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 1: case 4: //Vapor or default
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? input[idx] : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? input[idx] : input[idx + 1];
		d = neigh[idx].down == GROUND ? input[idx] : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 2: //Vel X
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? 0.0f : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? 0.0f : input[idx + 1];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; //No-Slip
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	case 3: //Vel Y
		l = neigh[idx].left == OUTSIDE ? input[idx] : neigh[idx].left == GROUND ? 0.0f : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? input[idx] : neigh[idx].right == GROUND ? 0.0f : input[idx + 1];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; // No-Slip
		u = neigh[idx].up == OUTSIDE ? 0.0f : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	}

	output[idx] = (input[idx] + k * (l + r + u + d)) / (1 + 4 * k);
}

__global__ void diffuseBlack(const Neigh* neigh, const float* groundT, const float* pressures, const float* groundP, const float* defaultVal, 
	const float* input, float* output, const float k, const int type)
{
	if (isGroundGPU()) return;

	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if ((tX + tY) % 2 == 0) return;

	const int idx = tX + tY * GRIDSIZESKYX;
	const int idxGL = tX - 1;
	const int idxGR = tX + 1;
	const int idxGD = tX;
	float l = 0.0f;
	float r = 0.0f;
	float d = 0.0f;
	float u = 0.0f;
	switch (type)
	{
	case 0: //Temp
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGL] - 273.25f, pressures[tY], groundP[idxGL]) + 273.15f) : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND && isGroundLevel() ? (potentialTempGPU(groundT[idxGR] - 273.25f, pressures[tY], groundP[idxGR]) + 273.15f) : input[idx + 1];
		d = neigh[idx].down == GROUND ? potentialTempGPU(groundT[idxGD] - 273.25f, pressures[tY], groundP[idxGD]) + 273.15f : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 1: case 4: //Vapor or default
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? input[idx] : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? input[idx] : input[idx + 1];
		d = neigh[idx].down == GROUND ? input[idx] : input[idx - GRIDSIZESKYX];
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX];
		break;
	case 2: //Vel X
		l = neigh[idx].left == OUTSIDE ? defaultVal[tY] : neigh[idx].left == GROUND ? 0.0f : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? defaultVal[tY] : neigh[idx].right == GROUND ? 0.0f : input[idx + 1];
		d = neigh[idx].down == GROUND ? 0.0f : input[idx - GRIDSIZESKYX]; //No-Slip
		u = neigh[idx].up == OUTSIDE ? defaultVal[tY] : input[idx + GRIDSIZESKYX]; // Free-Slip
		break;
	case 3: //Vel Y
		l = neigh[idx].left == OUTSIDE ? input[idx] : neigh[idx].left == GROUND ? 0.0f : input[idx - 1];
		r = neigh[idx].right == OUTSIDE ? input[idx] : neigh[idx].right == GROUND ? 0.0f : input[idx + 1];
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

__device__ float advectPPMFlux(const float* array, const float defaultVal, const float velfield, const Neigh neighbour, const Neigh downWindNeigh, 
	const float dt, const int tX, const bool x, const bool isRight)
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

__global__ void advectPPMX(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldX,
	const Neigh* __restrict__  neighbour,
	const float dt)
{

	if (isGroundGPU()) return;

	__shared__ float sArray[GRIDSIZESKYX];
	__shared__ float sVelX[GRIDSIZESKYX];

	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tY * GRIDSIZESKYX + tX;

	if (idx >= GRIDSIZESKY) return;

	sArray[tX] = arrayIn[idx];
	sVelX[tX] = velfieldX[idx];

	__syncthreads();

	Neigh neigh = neighbour[idx];
	float defVal = 0.0f;
	if (defaultVal)
	{
		defVal = defaultVal[tY];
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

__global__ void advectPPMY(const float* __restrict__ arrayIn,
	float* __restrict__ arrayOut,
	const float* __restrict__ defaultVal,
	const float* __restrict__ velfieldY,
	const Neigh* __restrict__  neighbour,
	const float dt)
{
	if (isGroundGPU()) return;

	__shared__ float sArray[GRIDSIZESKYY];
	__shared__ float sVelY[GRIDSIZESKYY];

	int tY = threadIdx.x;
	int idxY = blockIdx.x;
	int idx = tY * GRIDSIZESKYX + idxY;

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

__device__ float3 calculateFallingVel(const float* Qr, const float* Qs, const float* Qi, const float densAir, const int type)
{
	const int idx = threadIdx.x + blockIdx.x * GRIDSIZESKYX;

	bool all{ (type == 4) };
	float UR = 0.0f;
	float US = 0.0f;
	float UI = 0.0f;

	if (type == 1 || all)
	{
		const float a = 2115.0f;
		const float b = 0.8f;
		const float gammaR = tgammaf(4.0f + b); //TODO: Save it, since it is a constant
		float slopeR = slopePrecipGPU(densAir, Qr[idx], 0);
		UR = a * (gammaR / (6 * powf(slopeR, b))) * sqrtf(1.225f / densAir);
		UR *= 0.01f; //Convert cm to m
	}
	else if (type == 2 || all)
	{
		const float c = 152.93f;
		const float d = 0.25f;
		const float gammaS = tgammaf(4.0f + d); //TODO: Save it, since it is a constant
		float slopeS = slopePrecipGPU(densAir, Qs[idx], 1);
		US = c * (gammaS / (6 * powf(slopeS, d))) * sqrtf(1.225f / densAir);
		US *= 0.01f; //Convert cm to m
	}
	else if (type == 3 || all)
	{
		//Densities in g/cm3
		const float densI = 0.91f;
		//constants
		const float CD = 0.6f; //Drag coefficient
		const float gammaI = tgammaf(4.0f + 0.25f); //TODO: Save it, since it is a constant
		float slopeI = slopePrecipGPU(densAir, Qi[idx], 2);
		UI = (gammaI / (6 * powf(slopeI, 0.5f))) * powf(4 * ConstantsGPU::g * 100 * densI / (3 * CD * densAir * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3
		UI *= 0.01f; //Convert cm to m
	}
	return float3{ UR, US, UI };
}

__global__ void advectPrecipRed(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi, 
	const float* pressures, const float* groundP, const int type, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if ((tX + tY) % 2 == 0) return; //When 0 we return at red

	const int idx = tX + tY * GRIDSIZESKYX;

	float fallVel = 0.0f;
	float fallVelUp = 0.0f;

	float3 fallVelocitiesPrecip{ 0.0f };
	float3 fallVelocitiesPrecipUP{ 0.0f };

	{
		const float T = float(potTemp[idx]) * powf(pressures[tY] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressures[tY] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecip = calculateFallingVel(Qr, Qs, Qi, density, type);
	}
	if (neigh[idx].up == SKY)
	{
		const int iUP = idx + GRIDSIZESKYX;
		const int yUP = tY + 1;
		const float T = float(potTemp[iUP]) * powf(pressures[yUP] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[iUP] + 1);
		const float density = pressures[yUP] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecipUP = calculateFallingVel(Qr, Qs, Qi, density, type);
	}
	switch (type)
	{
	case 1:
		fallVel = fallVelocitiesPrecip.x;
		fallVelUp = fallVelocitiesPrecipUP.x;
		break;
	case 2:
		fallVel = fallVelocitiesPrecip.y;
		fallVelUp = fallVelocitiesPrecipUP.y;
		break;
	case 3:
		fallVel = fallVelocitiesPrecip.z;
		fallVelUp = fallVelocitiesPrecipUP.z;
		break;
	default:
		break;
	}

	if (neigh[idx].up == SKY)
	{
		array[idx] += dt * fmin((fallVelUp / VOXELSIZE) * array[idx + GRIDSIZESKYX], array[idx + GRIDSIZESKYX]); //Grab % of precip above
	}
	array[idx] -= dt * fmin((fallVel / VOXELSIZE) * array[idx], array[idx]); //Remove % of precip
}

__global__ void advectPrecipBlack(float* array, const Neigh* neigh, const float* potTemp, const float* Qv, const float* Qr, const float* Qs, const float* Qi,
	const float* pressures, const float* groundP, const int type, const float dt)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	if ((tX + tY) % 2 == 1) return; //When 1 we return at black

	const int idx = tX + tY * GRIDSIZESKYX;

	float fallVel = 0.0f;
	float fallVelUp = 0.0f;

	float3 fallVelocitiesPrecip{ 0.0f };
	float3 fallVelocitiesPrecipUP{ 0.0f };

	{
		const float T = float(potTemp[idx]) * powf(pressures[tY] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[idx] + 1);
		const float density = pressures[tY] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecip = calculateFallingVel(Qr, Qs, Qi, density, type);
	}
	if (neigh[idx].up == SKY)
	{
		const int iUP = idx + GRIDSIZESKYX;
		const int yUP = tY + 1;
		const float T = float(potTemp[iUP]) * powf(pressures[yUP] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
		const float Tv = T * (0.608f * Qv[iUP] + 1);
		const float density = pressures[yUP] * 100 / (ConstantsGPU::Rsd * Tv); //Convert Pha to Pa
		fallVelocitiesPrecipUP = calculateFallingVel(Qr, Qs, Qi, density, type);
	}
	switch (type)
	{
	case 1:
		fallVel = fallVelocitiesPrecip.x;
		fallVelUp = fallVelocitiesPrecipUP.x;
		break;
	case 2:
		fallVel = fallVelocitiesPrecip.y;
		fallVelUp = fallVelocitiesPrecipUP.y;
		break;
	case 3:
		fallVel = fallVelocitiesPrecip.z;
		fallVelUp = fallVelocitiesPrecipUP.z;
		break;
	default:
		break;
	}

	if (neigh[idx].up == SKY)
	{
		array[idx] += dt * fmin((fallVelUp / VOXELSIZE) * array[idx + GRIDSIZESKYX], array[idx + GRIDSIZESKYX]); //Grab % of precip above
	}
	array[idx] -= dt * fmin((fallVel / VOXELSIZE) * array[idx], array[idx]); //Remove % of precip
}

__device__ void applyPreconditionerGPU(float* output, const float precon, const float div)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;
	if (isGroundGPU(tX, tY))
	{
		output[idx] = 0.0f;
		return;
	}

	output[idx] = div * precon;
}

__device__ void dotProductGPU(float* result, const float* a, const float* b)
{
	//To speed up dot product, we make all blocks sum up their values and then connect them together.
	//This is faster than 1 thead doing all the work.
	__shared__ float sresult[GRIDSIZESKYX];
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	sresult[tX] = a[idx] * b[idx];
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
		printf("result1[%i] = %f\n", tY, sresult[0]);
		atomicAdd(result, sresult[0]);
	}
}

__device__ void applyAGPU(float* output, const float* input, const char3 A, const char3 ADo, const char3 ALe)
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

	output[idx] = A.z * input[idx] +
					((ALe.x * SLe +
						ADo.y * SDo +
						A.x * SRi +
						A.y * SUp)
					);
}

__device__ float calculateDivergenceGPU(const Neigh neigh, const float* velX, const float* velY)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	if (isGroundGPU(tX, tY)) return 0.0f;

	//If Right or Left at the outside cell. We use the default values.
	//If At Ceiling, we don't have to do anything: y is already set to 0, and ceiling is free-slip
	//If at Ground, we set velocity to 0, creating divergence, making sure this is no-slip.

	const float Ucurr = (neigh.right == OUTSIDE && idx != 0) ? defaultVel[tY] : (neigh.right == GROUND ? 0.0f : velX[idx]);
	const float Umin1 = (neigh.left == OUTSIDE || idx == 0) ? defaultVel[tY] : (neigh.left == GROUND ? 0.0f : velX[idx - 1]);
	const float Vcurr = velY[idx];
	const float Vmin1 = (neigh.down == SKY && idx - GRIDSIZESKYX >= 0) ? velY[idx - GRIDSIZESKYX] : 0.0f;

	return ((Ucurr - Umin1) + (Vcurr - Vmin1));
}

__global__ void calculatePresProjGPU(float* outputArray, const Neigh* neigh, const float* velX, const float* velY, const float* precon, float* divergence, 
	float* z, float* s, const char3* A, float* Sresult1, float* Sresult2)
{
	const int tX = threadIdx.x;
	const int tY = blockIdx.x;
	const int idx = tX + tY * GRIDSIZESKYX;

	const float tolValue = 1e-6f;
	const int MAXITERATION = 200;
	__shared__ bool done;

	//Reset values
	z[idx] = 0.0f;
	s[idx] = 0.0f;
	outputArray[idx] = 0.0f;

	if (idx == 0)
	{
		*Sresult1 = 0.0f;
		*Sresult2 = 0.0f;
		done = false;
	}
	__syncthreads();

	//Prevent more lookups by already getting the A's we need.
	char3 ALeft = (neigh[idx].left == SKY) ? A[idx - 1] : char3{ 0, 0, 0 };
	char3 ADown = (neigh[idx].down == SKY) ? A[idx - GRIDSIZESKYX] : char3{ 0, 0, 0 };
	char3 ACur = A[idx];

	divergence[idx] = calculateDivergenceGPU(neigh[idx], velX, velY);

	applyPreconditionerGPU(z, precon[idx], divergence[idx]);

	//Checking max residual (first thread)
	if (idx == 0)
	{
		float maxr = 0.0f;
		for (int y = 0; y < GRIDSIZESKYY; y++)
		{
			for (int x = 0; x < GRIDSIZESKYX; x++)
			{
				if (isGroundGPU(x, y)) continue;
				maxr = fmax(maxr, fabsf(divergence[x + y * GRIDSIZESKYX]));
			}
		}
		if (maxr == 0) done = true;
	}
	if (idx == 0) *Sresult1 = 0.0f;
	__syncthreads();

	dotProductGPU(Sresult1, z, divergence);
	s[idx] = z[idx];

	//Lets re-ensemble
	__syncthreads();

	if (done)
	{
		return;
	}

	for (int i = 0; i < 1; i++)
	{
		applyAGPU(z, s, ACur, ADown, ALeft);
		__syncthreads();

		if (idx == 0)
		{
			*Sresult2 = 0.0f;
		}
		__syncthreads();

		//Dotproduct
		dotProductGPU(Sresult2, z, s);
		__syncthreads();
		if (idx == 0)
		{
			printf("sresult = %4.20f\n", *Sresult2);
		}
		//if (idx == 0) {
		//	printf("Iter %d: Sresult1=%f, Sresult2=%f\n",
		//		i, *Sresult1, *Sresult2);
		//}
		if (idx == 0)
		{		
			*Sresult2 = *Sresult1 / *Sresult2;
		}
		__syncthreads();

		//Adding up pressure value and reducing residual value
		if (!isGroundGPU(tX, tY))
		{
			outputArray[idx] += *Sresult2 * s[idx];
			divergence[idx] -= *Sresult2 * z[idx];
		}

		//Checking max residual (first thread)
		if (idx == 0)
		{
			float maxr = 0.0f;
			for (int y = 0; y < GRIDSIZESKYY; y++)
			{
				for (int x = 0; x < GRIDSIZESKYX; x++)
				{
					if (isGroundGPU(x, y)) continue;
					maxr = fmax(maxr, fabsf(divergence[x + y * GRIDSIZESKYX]));
				}
			}
			if (maxr < tolValue) done = true;
		}
		__syncthreads();

		if (done) return;

		applyPreconditionerGPU(z, precon[idx], divergence[idx]);
		__syncthreads();

		if (idx == 0)
		{
			*Sresult2 = 0.0f;
		}		
		__syncthreads();

		//Dotproduct
		dotProductGPU(Sresult2, z, divergence);
		__syncthreads();

		if (idx == 0)
		{
			*Sresult1 = *Sresult2 / *Sresult1;
		}
		__syncthreads();

		//Setting search vector s
		s[idx] = z[idx] + *Sresult1 * s[idx];

		if (idx == 0)
		{
			*Sresult1 = *Sresult2;
		}
		__syncthreads();

	}
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

//-------------------------------------OTHER-------------------------------------

__global__ void calculateCloudCover(float* output, const float* Qc, const float* Qw)
{
	const int tX = threadIdx.x;

	float totalCloudContent = 0.0f;
	const float qfull = 1.2f; // a threshold value where all incoming radiation is reflected by cloud matter: http://meto.umd.edu/~zli/PDF_papers/Li%20Nature%20Article.pdf
	for (int y = 0; y < GRIDSIZESKYY; y++) totalCloudContent += (Qc[tX + y * GRIDSIZESKYX] + Qw[tX + y * GRIDSIZESKYX]) * VOXELSIZE;
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
	const int maxRight = distXR > GRIDSIZEGROUND ? GRIDSIZEGROUND : distXR;
	const int minLeft = distXL < 0 ? -1 : distXL;
	const float MDistanceSqr = 1 / (maxDistance * maxDistance);
	const int distOYY = (oY - lY) * (oY - lY);

	//TODO: when difference in ground pressure, remove optimization:
	float potTempMultiplier = 1.0f;// temp ? powf((pressures[lY] / groundP[0]), ConstantsGPU::Rsd / ConstantsGPU::Cpd) : 1.0f;

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
	if (amount == 0) return 0.0f;

	return average / amount;
}

__global__ void buoyancyGPU(float* velY, const Neigh* neigh, const float* potTemp, const float* Qv, 
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

	//Now check if we are on the edges, if so, we leave
	if (neigh[idx].left == OUTSIDE || neigh[idx].right == OUTSIDE || neigh[idx].up == OUTSIDE || neigh[idx].down == OUTSIDE)
	{
		return;
	}

	//Vapor environment and Vapor Parcel
	float Qenv = 0.0f, QenvUp = 0.0f, QenvDown = 0.0f;
	float QP = 0.0f, QPUp = 0.0f, QPDown = 0.0f;

	const float T = static_cast<float>(potTemp[idx]) * powf(pressures[tY] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
	const float T2 = static_cast<float>(potTemp[idx + GRIDSIZESKYX]) * powf(pressures[tY + 1] / groundP[tX], ConstantsGPU::Rsd / ConstantsGPU::Cpd);

	// ------------ QV ------------
	if (down)
	{
		QenvDown = calculateLayerAverage(sharedLayerDown, pressures, groundP, maxDistance, tY - 1, false);
		QPDown = Qv[idx - GRIDSIZESKYX];
	}
	Qenv = calculateLayerAverage(sharedLayer, pressures, groundP, maxDistance, tY, false);
	QP = Qv[idx];
	if (up)
	{
		QenvUp = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY + 1, false);
		QPUp = Qv[idx + GRIDSIZESKYX];
	}


	// ------------ Temp ------------

	// Re-use layers
	sharedLayer[tX] = potTemp[idx];
	sharedLayerUp[tX] = up ? potTemp[idx + GRIDSIZESKYX] : -1.0f;
	sharedLayerDown[tX] = down ? potTemp[idx - GRIDSIZESKYX] : -1.0f;

	//Wait for every thread to be done (for sure).
	__syncthreads();


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
			TadiabDown = T - MLRGPU(T - 273.15f, pressures[tY - 1]) * (pressures[tY] - pressures[tY - 1]);
			TenvDown = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY - 1, true);
		}
		if (up)
		{
			TadiabUp = MLRGPU(T - 273.15f, pressures[tY + 1]) * (pressures[tY + 1] - pressures[tY]) + T;
			TenvUp = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY + 1, true);
		}
	}
	else
	{
		//Air is dry, thus we use dry adiabatic
		if (down)
		{
			TadiabDown = T * powf(pressures[tY - 1] / pressures[tY], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			TenvDown = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY - 1, true);
		}
		if (up)
		{
			TadiabUp = T * powf(pressures[tY + 1] / pressures[tY], ConstantsGPU::Rsd / ConstantsGPU::Cpd);
			TenvUp = calculateLayerAverage(sharedLayerUp, pressures, groundP, maxDistance, tY + 1, true);
		}
	}
	Tenv = calculateLayerAverage(sharedLayer, pressures, groundP, maxDistance, tY, true);

	//buoyancyStor[idx] = Tenv;


	// ------------ Buoyancy ------------

	// Seperate buoyancies
	float B = 0.0f, BUp = 0.0f, BDown = 0.0f;

	if (down)
	{
		//Temp parcel and Temp environment
		const float VTP = TadiabDown * (1.0f + 0.608f * QPDown);
		const float VTE = TenvDown * (1.0f + 0.608f * QenvDown);
		BDown = ConstantsGPU::g * ((VTP - VTE) / VTE);
	}
	{
		//Temp parcel and Temp environment
		const float VTP = Tadiab * (1.0f + 0.608f * QP);
		const float VTE = Tenv * (1.0f + 0.608f * Qenv);
		B = ConstantsGPU::g * ((VTP - VTE) / VTE);
	}
	if (up)
	{
		//Temp parcel and Temp environment
		const float VTP = TadiabUp * (1.0f + 0.608f * QPUp);
		const float VTE = TenvUp * (1.0f + 0.608f * QenvUp);
		BUp = ConstantsGPU::g * ((VTP - VTE) / VTE);
	}


	// Now we calculate if we use up or down, this depends if the parcel on the current levels is going up or down

	float buoyancyFinal = 0.0f;
	int divideBy = 1;

	if (B < 0.0f) //If falling
	{
		buoyancyFinal += BDown;
		divideBy++;
		if (buoyancyFinal > 0.0f) //Rising due to falling into colder air, so we reverse
		{
			buoyancyFinal -= BDown;
			divideBy--;
		}
	}
	else if (B > 0.0f) //If going up
	{
		buoyancyFinal += BUp;
		divideBy++;
		if (buoyancyFinal < 0.0f) //Falling due to rising into warmer air, so we reverse
		{
			buoyancyFinal -= BUp;
			divideBy--;
		}
	}
	else //If not moving based on temperature difference to the left or right
	{
		buoyancyFinal += BDown;
		buoyancyFinal += BUp;
		divideBy++;
		divideBy++;
	}

	buoyancyStor[idx] = buoyancyFinal / divideBy * dt;
	velY[idx] += buoyancyFinal / divideBy * dt;
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

	if (isGroundGPU())
	{
		precon[idx] = 0.0f;
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


//-------------------------------------HELPER-------------------------------------


__global__ void resetVelPressProj(const Neigh* neigh, float* velX, float* velY)
{
	int tX = threadIdx.x;
	int tY = blockIdx.x;
	int idx = tX + tY * GRIDSIZESKYX;

	if (neigh[idx].up != SKY)
	{
		velY[idx] = 0.0f;
	}
	if (neigh[idx].right != SKY)
	{
		velX[idx] = defaultVel[tY];
	}
	if (tX == 16 && tY == 16)
	{
		velY[idx] = 2.0f;
	}

}

__device__ bool isGroundLevel()
{
	int x = threadIdx.x;
	int y = blockIdx.x;
	return y == (GHeight[x] - 1);
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