#pragma once
#include <stdint.h> // uint32_t

//Do we want to use the GPU? (CPU is very unstable if not working at all atm)
#define USE_GPU 1

#define GRIDSIZESKYX 64
#define GRIDSIZESKYY 64
#define GRIDSIZESKYZ 64

#define GRIDSIZESKY (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ)
#define GRIDSIZEGROUND (GRIDSIZESKYX * GRIDSIZESKYZ)
#define VOXELSIZE 64.0f //Meters

// Data graph
#define MAXGRAPHLENGTH 100

#define BOUNDSTEMP boundsEnv{ CUSTOM, CUSTOM, CUSTOM}
#define BOUNDSVAPOR boundsEnv{ CUSTOM, CUSTOM, DIRICHLET}
#define BOUNDSVELXZ boundsEnv{ CUSTOM, CUSTOM, DIRICHLET}
#define BOUNDSVELY boundsEnv{ NEUMANN, NEUMANN, DIRICHLET}
#define BOUNDSAPPLYA boundsEnv{ NEUMANN, DIRICHLET, DIRICHLET }
#define BOUNDSDENSITY boundsEnv{ NEUMANN, NEUMANN, DIRICHLET }
#define BOUNDSBUOYANCY boundsEnv{ DIRICHLET, DIRICHLET, DIRICHLET }
#define BOUNDSPRESPROJ boundsEnv{ DIRICHLET, NEUMANN, NEUMANN }


// Wrapper for CPU/GPU functions
#ifdef __CUDACC__
#define HOSTDEVICE __host__ __device__
#else
#define HOSTDEVICE
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
	HOSTDEVICE void setCurrent(bool outside, envType type) { data &= ~(resetMask << 0); data |= (uint32_t(outside) << 0); data |= ((uint32_t(type) & envTypeMask) << 1); }
	HOSTDEVICE void setBackward(bool outside, envType type) { data &= ~(resetMask << 3); data |= (uint32_t(outside) << 3); data |= ((uint32_t(type) & envTypeMask) << 4); }
	HOSTDEVICE void setForward(bool outside, envType type)  { data &= ~(resetMask << 6); data |= (uint32_t(outside) << 6); data |= ((uint32_t(type) & envTypeMask) << 7); }
	HOSTDEVICE void setDown(bool outside, envType type)     { data &= ~(resetMask << 9); data |= (uint32_t(outside) << 9); data |= ((uint32_t(type) & envTypeMask) << 10); }
	HOSTDEVICE void setUp(bool outside, envType type)       { data &= ~(resetMask << 12); data |= (uint32_t(outside) << 12); data |= ((uint32_t(type) & envTypeMask) << 13); }
	HOSTDEVICE void setRight(bool outside, envType type)    { data &= ~(resetMask << 15); data |= (uint32_t(outside) << 15); data |= ((uint32_t(type) & envTypeMask) << 16); }
	HOSTDEVICE void setLeft(bool outside, envType type)     { data &= ~(resetMask << 18); data |= (uint32_t(outside) << 18); data |= ((uint32_t(type) & envTypeMask) << 19); }

	// Get if value is outside the grid
	HOSTDEVICE bool getOutsideCurrent()  const { return bool(data & (1u << 0)); }
	HOSTDEVICE bool getOutsideBackward() const { return bool(data & (1u << 3)); }
	HOSTDEVICE bool getOutsideForward()  const { return bool(data & (1u << 6)); }
	HOSTDEVICE bool getOutsideDown()     const { return bool(data & (1u << 9)); }
	HOSTDEVICE bool getOutsideUp()       const { return bool(data & (1u << 12)); }
	HOSTDEVICE bool getOutsideRight()    const { return bool(data & (1u << 15)); }
	HOSTDEVICE bool getOutsideLeft()     const { return bool(data & (1u << 18)); }

	// Get the environment type of the value
	HOSTDEVICE envType getEnvTypeCurrent()  const { return envType(char(data >> 1 & 3u)); }
	HOSTDEVICE envType getEnvTypeBackward() const { return envType(char(data >> 4 & 3u)); }
	HOSTDEVICE envType getEnvTypeForward()  const { return envType(char(data >> 7 & 3u)); }
	HOSTDEVICE envType getEnvTypeDown()     const { return envType(char(data >> 10 & 3u)); }
	HOSTDEVICE envType getEnvTypeUp()       const { return envType(char(data >> 13 & 3u)); }
	HOSTDEVICE envType getEnvTypeRight()    const { return envType(char(data >> 16 & 3u)); }
	HOSTDEVICE envType getEnvTypeLeft()     const { return envType(char(data >> 19 & 3u)); }

	HOSTDEVICE uint32_t getData() { return data; }

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
	POTTEMP, QV, QW, QC, QR, QS, QI, WIND, PGROUND, PRESSURE, DEBUG1, DEBUG2, DEBUG3
};

struct simInfo
{
	Neigh* neighbourData{ nullptr }; // Neighbouring data
};