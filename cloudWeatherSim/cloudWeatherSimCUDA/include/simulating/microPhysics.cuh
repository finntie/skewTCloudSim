#pragma once

#include "config.hpp"

void initGammasMicroPhysics(void* stream);

void setConstantsMicroPhys(float invBlockSpreadDepth, void* stream);


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
		*this = microPhysicsParams{};
	}

};

//Functions
__global__ void calculateEnvMicroPhysicsGPU(simParams p, float* _Qv, float* _Qw, float* _Qc, float* _Qr, float* _Qs, float* _Qi,
	const float _dt, const float _speed, const float* _temp, const float* _densAir, const float* _pressure, const int* _groundHeight, const float* _groundpressure,
	float* condens, float* depos, float* freeze, 
	const bool graphActive, const int3 minSelectPos, const int3 maxSelectPos, microPhysicsParams& microPhysicsResult);

__global__ void calculateGroundMicroPhysicsGPU(simParams p, float* _Qrs, float* _Qv,	float* _Qgr, float* _Qgs, float* _Qgi,
	const float _dt, const float _speed, float* _tempGround, const float* _tempAir, const float* _densAir, const float* _pressure, const float* _groundPressure,
	float* _time, const float _irradiance, const float* _windSpeedX, const float* _cloudCover,
	const int* _GHeight);

__global__ void calculatePrecipHittingGroundMicroPhysicsGPU(simParams p, float* _Qv, float* Qr, float* Qs, float* Qi, float* _Qgr, float* _Qgs, float* _Qgi,
	const float dt, const float _speed, float* _tempGround, const float* tempAir, const float* _densAir, const float* _pressure, const float* _groundPressure, 
	const float* _windSpeedX, const int* _GHeight);





