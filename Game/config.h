#pragma once

#define GRIDSIZESKYX 32
#define GRIDSIZESKYY 32
#define GRIDSIZESKYZ 32

#define GRIDSIZESKY (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ)
#define GRIDSIZEGROUND (GRIDSIZESKYX * GRIDSIZESKYZ)
#define VOXELSIZE 64.0f //Meters

// Data graph
#define MAXGRAPHLENGTH 100


//Storage array for setting boundary conditions easier.
enum envType
{
	SKY, OUTSIDE, GROUND
};

//Struct containing neighbouring values, if they are SKY, OUTSIDE or GROUND
struct Neigh
{
	//With forward being +z and backward -z
	envType left, right, up, down, forward, backward;
};

//Boundary condition
enum boundCon
{
	NEUMANN, DIRICHLET, CUSTOM
};

// Parameter type
enum parameter
{
	POTTEMP, QV, QW, QC, QR, QS, QI, WIND, PGROUND, PRESSURE, DEBUG1, DEBUG2, DEBUG3
};