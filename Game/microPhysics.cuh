#pragma once

#include <cuda_runtime.h> 

void initGammasMicroPhysics();

//Functions
__global__ void calculateEnvMicroPhysicsGPU(float* _Qv, float* _Qw, float* _Qc, float* _Qr, float* _Qs, float* _Qi,
	const float _dt, const float _speed, const float* _temp, const float* _pressure, const int* _groundHeight, const float* _groundpressure,
	float* condens, float* depos, float* freeze);

__global__ void calculateGroundMicroPhysicsGPU(float* _Qrs, float* _Qv,	float* _Qgr, float* _Qgs, float* _Qgi,
	const float _dt, const float _speed, float* _tempGround, const float* _tempAir, const float* _pressure, const float* _groundPressure,
	float* _time, const float _irradiance, const float* _windSpeedX, const float* _cloudCover,
	const int* _GHeight);

__global__ void calculatePrecipHittingGroundMicroPhysicsGPU(float* _Qv, float* Qr, float* Qs, float* Qi, float* _Qgr, float* _Qgs, float* _Qgi,
	const float dt, const float _speed, float* _tempGround, const float* tempAir, const float* _pressure, const float* _groundPressure, 
	const float* _windSpeedX, const int* _GHeight);


__device__ float FPVCON(const float temp, const float ps, const float Qv, const float QWS, const float dt, const float speed);
__device__ float FPVDEP(const float temp, const float ps, const float Qv, const float QWI, const float dt, const float speed);

__device__ float FPIMLT(const float Qc); //TODO: limit melting?
__device__ float FPIDW(const float dt, const float temp, const float Qc, const float Qw, const float Dair, const float ps);
__device__ float FPIHOM(const float temp, const float Qw);
__device__ float FPIACR(const float Qr, const float Qc, const float Dair, const float slopeR);
__device__ float FPRACI(const float Qr, const float Qc, const float Dair, const float slopeR);
__device__ float FPRAUT(const float Qw, const float QwMin, const float temp);
__device__ float FPRACW(const float Qr, const float Qw, const float Dair, const float slopeR);
__device__ float FPREVP(const float temp, const float Qr, const float Qv, const float QWS, const float Dair, const float ps, const float slopeR, const float PTerm1);
__device__ float FPRACS(const float Qr, const float Qs, const float PTerm2, const float3 fallVel, const float Dair, const float slopeS, const float slopeR);
__device__ float FPSACW(const float Qs, const float Qw, const float Dair, const float slopeS);
__device__ float FPSACR(const float Qr, const float Qs, const float3 fallVel, const float Dair, const float slopeR, const float slopeS);
__device__ float FPSACI(const float temp, const float Qs, const float Qc, const float Dair, const float slopeS);
__device__ float FPSAUT(const float temp, const float Qc, const float QcMin);
__device__ float FPSFW(const float dt, const float temp, const float ps, const float Qs, const float Qw, const float Qc, const float Qv, const float QWI, const float Dair); //TODO: use this function?
__device__ float FPSFI(const float dt, const float temp, const float ps, const float Qs, const float Qw, const float Qc, const float Qv, const float QWI, const float Dair); //TODO: also use old formula?
__device__ float FPSDEP(const float temp, const float ps, const float Qv, const float QWI, const float Dair, const float slopeS);
__device__ float FPSSUB(const float PSDEP);
__device__ float FPSMLT(const float temp, const float ps, const float Qs, const float Qv, const float Dair, const float slopeS, const float PSACW, const float PSACR);
__device__ float FPGAUT(const float temp, const float Qs, const float QiMin);
__device__ float FPGFR(const float temp, const float Qr, const float Dair, const float slopeR);
__device__ float FPGACW(const float Qi, const float Qw, const float slopeI, const float Dair);
__device__ float FPGACI(const float Qi, const float Qc, const float slopeI, const float Dair);
__device__ float FPGACR(const float Qi, const float Qr, const float Dair, const float3 fallVel, const float slopeR, const float slopeI);
__device__ float FPGACS(const float temp, const float Qs, const float Qi, const float Dair, const float3 fallVel, const float slopeS, const float slopeI, const bool EGS1 = false);
__device__ float FPGDRY(const float PGACW, const float PGACI, const float PGACR, const float PGACS);
__device__ float FPGSUB(const float temp, const float ps, const float Qv, const float QWI, const float Qi, const float Dair, const float slopeI);
__device__ float FPGMLT(const float temp, const float ps, const float Qi, const float Qv, const float Dair, const float slopeI, const float PGACW, const float PGACR);
__device__ float FPGWET(const float temp, const float ps, const float Qs, const float Qi, const float Qv, const float3 fallVel, const float Dair, const float slopeS, const float slopeI, const float PGACI, const float PGACS);
__device__ float FPGACR1(const float temp, const float Qs, const float Qi, const float Dair, const float3 fallVel, const float slopeS, const float slopeI, const float PGWET, const float PGACW, const float PGACI, const float PGACS);

//Ground variables
__device__ float FPGFLW(const float Qr);
__device__ float FPGGMLT(const float tempGroundC, const float Qi, const float Dair, const float slopeI);
__device__ float FPGSMLT(const float tempAirK, const float tempGroundC, const float ps, const float Qs, const float Qv, const float Qr, 
						const float irradiance, const float cloudCover, const float Dair, const float windSpeed, const float dt);
__device__ float FPGREVP(const float tempAirK, const float tempGroundC, const float ps, const float Qv, const float Qr, 
						const float irradiance, const float Dair, const float windSpeed);
__device__ float FPGDEVP(float* time, const float Qr, const float Qs, const float Qi, const float Qrs);
__device__ float FPGGFR(const float tempAirK, const float tempGroundC, const float ps, const float Qr, const float Qv, const float Qi, 
						const float Dair, const float irradiance, const float cloudCover, const float windSpeed, const float dt);
__device__ float FPGSFR(const float tempAirK, const float tempGroundC, const float ps, const float Qv, const float Qr, const float Qs, 
						const float Dair, const float windSpeed, const float dt);

//Precip hitting ground variables
__device__ float FPGRFR(const float tempAirK, const float tempGroundC, const float ps, const float Qr, const float Qv, const float Qi, const float Qgr, const float Qgs,
						const float Dair, const float windSpeed, const float3 fallVel);


