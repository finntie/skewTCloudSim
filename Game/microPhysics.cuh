#pragma once

#include <cuda_runtime.h> 

void initGammasMicroPhysics();

struct microPhysicsParams
{
	float PVCON, PVDEP, PIMLT,
		PIDW, PIHOM, PIACR,
		PRACI, PRAUT, PRACW,
		PREVP, PRACS, PSACW,
		PSACR, PSACI, PSAUT,
		PSFW, PSFI, PSDEP,
		PSSUB, PSMLT, PGAUT,
		PGFR, PGACW, PGACI,
		PGACR, PGDRY, PGACS,
		PGSUB, PGMLT, PGWET,
		PGACR1, PVVAP, PVSUB;

	__host__ __device__ void init(float pvcon, float pvdep, float pimlt,
		float pidw, float pihom, float piacr,
		float praci, float praut, float pracw,
		float prevp, float pracs, float psacw,
		float psacr, float psaci, float psaut,
		float psfw, float psfi, float psdep,
		float pssub, float psmlt, float pgaut,
		float pgfr, float pgacw, float pgaci,
		float pgacr, float pgdry, float pgacs,
		float pgsub, float pgmlt, float pgwet,
		float pgacr1, float pvvap, float pvsub)
	{
		PVCON = pvcon, PVDEP = pvdep, PIMLT = pimlt,
			PIDW = pidw, PIHOM = pihom, PIACR = piacr,
			PRACI = praci, PRAUT = praut, PRACW = pracw,
			PREVP = prevp, PRACS = pracs, PSACW = psacw,
			PSACR = psacr, PSACI = psaci, PSAUT = psaut,
			PSFW = psfw, PSFI = psfi, PSDEP = psdep,
			PSSUB = pssub, PSMLT = psmlt, PGAUT = pgaut,
			PGFR = pgfr, PGACW = pgacw, PGACI = pgaci,
			PGACR = pgacr, PGDRY = pgdry, PGACS = pgacs,
			PGSUB = pgsub, PGMLT = pgmlt, PGWET = pgwet,
			PGACR1 = pgacr1, PVVAP = pvvap, PVSUB = pvsub;
	}

	//Easy way to add two together.
	__host__ __device__ microPhysicsParams& operator+(const microPhysicsParams& other)
	{	
		PVCON += other.PVCON, PVDEP += other.PVDEP, PIMLT += other.PIMLT,
		PIDW += other.PIDW, PIHOM += other.PIHOM, PIACR += other.PIACR,
		PRACI += other.PRACI, PRAUT += other.PRAUT, PRACW += other.PRACW,
		PREVP += other.PREVP, PRACS += other.PRACS, PSACW += other.PSACW,
		PSACR += other.PSACR, PSACI += other.PSACI, PSAUT += other.PSAUT,
		PSFW += other.PSFW, PSFI += other.PSFI, PSDEP += other.PSDEP,
		PSSUB += other.PSSUB, PSMLT += other.PSMLT, PGAUT += other.PGAUT,
		PGFR += other.PGFR, PGACW += other.PGACW, PGACI += other.PGACI,
		PGACR += other.PGACR, PGDRY += other.PGDRY, PGACS += other.PGACS,
		PGSUB += other.PGSUB, PGMLT += other.PGMLT, PGWET += other.PGWET,
		PGACR1 += other.PGACR1, PVVAP += other.PVVAP, PVSUB += other.PVSUB;
		return *this;
	}

	__device__ void atomicAddValues(microPhysicsParams& result, microPhysicsParams& value)
	{
		atomicAdd(&result.PVCON, value.PVCON);
		atomicAdd(&result.PVDEP, value.PVDEP);
		atomicAdd(&result.PIMLT, value.PIMLT);
		atomicAdd(&result.PIDW, value.PIDW);
		atomicAdd(&result.PIHOM, value.PIHOM);
		atomicAdd(&result.PIACR, value.PIACR);
		atomicAdd(&result.PRACI, value.PRACI);
		atomicAdd(&result.PRAUT, value.PRAUT);
		atomicAdd(&result.PRACW, value.PRACW);
		atomicAdd(&result.PREVP, value.PREVP);
		atomicAdd(&result.PRACS, value.PRACS);
		atomicAdd(&result.PSACW, value.PSACW);
		atomicAdd(&result.PSACR, value.PSACR);
		atomicAdd(&result.PSACI, value.PSACI);
		atomicAdd(&result.PSAUT, value.PSAUT);
		atomicAdd(&result.PSFW, value.PSFW);
		atomicAdd(&result.PSFI, value.PSFI);
		atomicAdd(&result.PSDEP, value.PSDEP);
		atomicAdd(&result.PSSUB, value.PSSUB);
		atomicAdd(&result.PSMLT, value.PSMLT);
		atomicAdd(&result.PGAUT, value.PGAUT);
		atomicAdd(&result.PGFR, value.PGFR);
		atomicAdd(&result.PGACW, value.PGACW);
		atomicAdd(&result.PGACI, value.PGACI);
		atomicAdd(&result.PGACR, value.PGACR);
		atomicAdd(&result.PGDRY, value.PGDRY);
		atomicAdd(&result.PGACS, value.PGACS);
		atomicAdd(&result.PGSUB, value.PGSUB);
		atomicAdd(&result.PGMLT, value.PGMLT);
		atomicAdd(&result.PGWET, value.PGWET);
		atomicAdd(&result.PGACR1, value.PGACR1);
		atomicAdd(&result.PVVAP, value.PVVAP);
		atomicAdd(&result.PVSUB, value.PVSUB);
	}

	__host__ __device__ void reset()
	{
		memset(this, 0, sizeof(*this)); //This works?
	}

};

//Functions
__global__ void calculateEnvMicroPhysicsGPU(float* _Qv, float* _Qw, float* _Qc, float* _Qr, float* _Qs, float* _Qi,
	const float _dt, const float _speed, const float* _temp, const float* _densAir, const float* _pressure, const int* _groundHeight, const float* _groundpressure,
	float* condens, float* depos, float* freeze, 
	const bool graphActive, const int2 minSelectPos, const int2 maxSelectPos, microPhysicsParams& microPhysicsResult);

__global__ void calculateGroundMicroPhysicsGPU(float* _Qrs, float* _Qv,	float* _Qgr, float* _Qgs, float* _Qgi,
	const float _dt, const float _speed, float* _tempGround, const float* _tempAir, const float* _densAir, const float* _pressure, const float* _groundPressure,
	float* _time, const float _irradiance, const float* _windSpeedX, const float* _cloudCover,
	const int* _GHeight);

__global__ void calculatePrecipHittingGroundMicroPhysicsGPU(float* _Qv, float* Qr, float* Qs, float* Qi, float* _Qgr, float* _Qgs, float* _Qgi,
	const float dt, const float _speed, float* _tempGround, const float* tempAir, const float* _densAir, const float* _pressure, const float* _groundPressure, 
	const float* _windSpeedX, const int* _GHeight);


__device__ float FPVCON(const float temp, const float ps, const float Qv, const float QWS, const float dt, const float speed);

__device__ float FPIMLT(const float temp, const float Qc); //TODO: limit melting?
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
__device__ float FPSMLT(const float temp, const float ps, const float Qw, const float Qr, const float Qs, const float Qv, const float Dair, const float slopeS, const float PSACW, const float PSACR, const float dtSpeed);
__device__ float FPGAUT(const float temp, const float Qs, const float QiMin);
__device__ float FPGFR(const float temp, const float Qr, const float Dair, const float slopeR);
__device__ float FPGACW(const float Qi, const float Qw, const float slopeI, const float Dair);
__device__ float FPGACI(const float Qi, const float Qc, const float slopeI, const float Dair);
__device__ float FPGACR(const float Qi, const float Qr, const float Dair, const float3 fallVel, const float slopeR, const float slopeI);
__device__ float FPGACS(const float temp, const float Qs, const float Qi, const float Dair, const float3 fallVel, const float slopeS, const float slopeI, const bool EGS1 = false);
__device__ float FPGDRY(const float Qw, const float Qc, const float Qr, const float Qs, const float PGACW, const float PGACI, const float PGACR, const float PGACS, const float dtSpeed);
__device__ float FPGSUB(const float temp, const float ps, const float Qv, const float QWI, const float Qi, const float Dair, const float slopeI);
__device__ float FPGMLT(const float temp, const float ps, const float Qw, const float Qr, const float Qi, const float Qv, const float Dair, const float slopeI, const float PGACW, const float PGACR, const float dtSpeed);
__device__ float FPGWET(const float temp, const float ps, const float Qc, const float Qs, const float Qi, const float Qv, const float3 fallVel, const float Dair, const float slopeS, const float slopeI, const float PGACI, const float PGACS, const float dtSpeed);
__device__ float FPGACR1(const float temp, const float Qw, const float Qc, const float Qs, const float Qi, const float Dair, const float3 fallVel, const float slopeS, const float slopeI, const float PGWET, const float PGACW, const float PGACI, const float PGACS, const float dtSpeed);

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


