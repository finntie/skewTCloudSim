#include "pch.h"
#include "config.h"

// Only exists so the extern variables can be declared

int GRIDSIZESKYX = 256;
int GRIDSIZESKYY = 128;
int GRIDSIZESKYZ = 256;

int GRIDSIZESKY = (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ);
int GRIDSIZEGROUND = (GRIDSIZESKYX * GRIDSIZESKYZ);
float VOXELSIZE = 32.0f; // In Meters