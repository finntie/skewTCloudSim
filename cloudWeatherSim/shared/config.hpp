#pragma once
#include <stdint.h> // uint32_t

//Do we want to use the GPU? (CPU is very unstable if not working at all atm)
#define USE_GPU 1

inline int GRIDSIZESKYX = 128;
inline int GRIDSIZESKYY = 128;
inline int GRIDSIZESKYZ = 128;

extern int GRIDSIZESKY;
extern int GRIDSIZEGROUND;
extern float VOXELSIZE; // In Meters
z
// Data graph
#define MAXGRAPHLENGTH 100

#define BOUNDSTEMP boundsEnv{ CUSTOM, CUSTOM, CUSTOM}
#define BOUNDSVAPOR boundsEnv{ CUSTOM, CUSTOM, CUSTOM}
#define BOUNDSVELXZ boundsEnv{ CUSTOM, CUSTOM, DIRICHLET}
#define BOUNDSVELY boundsEnv{ NEUMANN, NEUMANN, DIRICHLET}
#define BOUNDSAPPLYA boundsEnv{ NEUMANN, DIRICHLET, DIRICHLET }
#define BOUNDSDENSITY boundsEnv{ NEUMANN, NEUMANN, DIRICHLET }
#define BOUNDSBUOYANCY boundsEnv{ DIRICHLET, DIRICHLET, DIRICHLET }
#define BOUNDSPRESPROJ boundsEnv{ NEUMANN, NEUMANN, NEUMANN } // NEUMANN in presProj means solid boundaries, DIRICHLET means free boundaries 
#define BOUNDSMIXINGRATIOS boundsEnv{ DIRICHLET, DIRICHLET, DIRICHLET}

// Wrapper for CPU/GPU functions
#ifdef __CUDACC__
#include "msvc_defines.h"
#include <hip/hip_runtime.h>
#else
#include "gpu_defines.h"
#endif

//Storage array for setting boundary conditions easier.
enum envType : unsigned char
{
	SKY, GROUND
};

struct singleNeigh
{
	singleNeigh() = default;
	singleNeigh(bool outside, envType type) : outside(outside), type(type) {}
	inline bool valid() { return (!outside && type == SKY); }
	bool outside{ false };
	envType type{ SKY };
};

//Struct containing neighbouring values
struct Neigh
{
	//With forward being +z and backward -z
	//singleNeigh left, right, up, down, forward, backward, current;
	static constexpr uint32_t resetMask = 7;
	static constexpr uint32_t envTypeMask = 3;

	// To set values:
	// 1. Reset part of the data we want to set, using a mask (for example: ~7u << 3 means grab inverse of 3 bits, move this 3 bits to the left, which creates a mask for our backward)
	// 2. Set the bool, simply using a or bitwise operation, shifting the bool first to the correct point.
	// 3. Only use first 2 bits of the envType, shift to correct place in data, then use or bitwise operation to set the data.
	__host__ __device__ void setCurrent(bool outside, envType type) { data &= ~(resetMask << 0); data |= (uint32_t(outside) << 0); data |= ((uint32_t(type) & envTypeMask) << 1); }
	__host__ __device__ void setBackward(bool outside, envType type) { data &= ~(resetMask << 3); data |= (uint32_t(outside) << 3); data |= ((uint32_t(type) & envTypeMask) << 4); }
	__host__ __device__ void setForward(bool outside, envType type)  { data &= ~(resetMask << 6); data |= (uint32_t(outside) << 6); data |= ((uint32_t(type) & envTypeMask) << 7); }
	__host__ __device__ void setDown(bool outside, envType type)     { data &= ~(resetMask << 9); data |= (uint32_t(outside) << 9); data |= ((uint32_t(type) & envTypeMask) << 10); }
	__host__ __device__ void setUp(bool outside, envType type)       { data &= ~(resetMask << 12); data |= (uint32_t(outside) << 12); data |= ((uint32_t(type) & envTypeMask) << 13); }
	__host__ __device__ void setRight(bool outside, envType type)    { data &= ~(resetMask << 15); data |= (uint32_t(outside) << 15); data |= ((uint32_t(type) & envTypeMask) << 16); }
	__host__ __device__ void setLeft(bool outside, envType type)     { data &= ~(resetMask << 18); data |= (uint32_t(outside) << 18); data |= ((uint32_t(type) & envTypeMask) << 19); }

	// Get if value is outside the grid
	__host__ __device__ bool getOutsideCurrent()  const { return bool(data & (1u << 0)); }
	__host__ __device__ bool getOutsideBackward() const { return bool(data & (1u << 3)); }
	__host__ __device__ bool getOutsideForward()  const { return bool(data & (1u << 6)); }
	__host__ __device__ bool getOutsideDown()     const { return bool(data & (1u << 9)); }
	__host__ __device__ bool getOutsideUp()       const { return bool(data & (1u << 12)); }
	__host__ __device__ bool getOutsideRight()    const { return bool(data & (1u << 15)); }
	__host__ __device__ bool getOutsideLeft()     const { return bool(data & (1u << 18)); }

	// Get the environment type of the value
	__host__ __device__ envType getEnvTypeCurrent()  const { return envType(char(data >> 1 & 3u)); }
	__host__ __device__ envType getEnvTypeBackward() const { return envType(char(data >> 4 & 3u)); }
	__host__ __device__ envType getEnvTypeForward()  const { return envType(char(data >> 7 & 3u)); }
	__host__ __device__ envType getEnvTypeDown()     const { return envType(char(data >> 10 & 3u)); }
	__host__ __device__ envType getEnvTypeUp()       const { return envType(char(data >> 13 & 3u)); }
	__host__ __device__ envType getEnvTypeRight()    const { return envType(char(data >> 16 & 3u)); }
	__host__ __device__ envType getEnvTypeLeft()     const { return envType(char(data >> 19 & 3u)); }

	__host__ __device__ uint32_t getData() { return data; }

private:
	uint32_t data;


};

enum direction
{
	LEFT, RIGHT, UP, DOWN, FORWARD, BACKWARD
};

//Boundary condition
enum boundCon
{
	NEUMANN, DIRICHLET, CUSTOM
};

///- Sides 
///- Up
///- Ground
///Holds bounds for different direction, downwards is ALWAYS ground
struct boundsEnv
{
	//Sides meaning left, right, forward and backwards
	boundCon sides;
	boundCon up;
	//Ground includes downwards and possibly left and righ when there is ground
	boundCon ground;
};

// Parameter type
enum parameter
{
	POTTEMP, QV, QW, QC, QR, QS, QI, WINDX, WINDY, WINDZ, PGROUND, PRESSURE, DEBUG1, DEBUG2, DEBUG3
};

enum parameterGround
{
	TEMP, QRS, QGR, QGS, QGI
};

struct simInfo
{
	Neigh* neighbourData{ nullptr }; // Neighbouring data
};