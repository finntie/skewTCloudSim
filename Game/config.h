#pragma once

#define GRIDSIZESKYX 32
#define GRIDSIZESKYY 32
#define GRIDSIZESKYZ 32

#define GRIDSIZESKY (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ)
#define GRIDSIZEGROUND (GRIDSIZESKYX * GRIDSIZESKYZ)
#define VOXELSIZE 256.0f //Meters

// Data graph
#define MAXGRAPHLENGTH 100


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
	singleNeigh left, right, up, down, forward, backward;
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