#pragma once

//Do we want to use the GPU? (CPU is very unstable if not working at all atm)
#define USE_GPU 1

#define GRIDSIZESKYX 128
#define GRIDSIZESKYY 128
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

//Struct containing neighbouring values, if they are SKY, OUTSIDE or GROUND
struct Neigh
{
	//With forward being +z and backward -z
	singleNeigh left, right, up, down, forward, backward, current;
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