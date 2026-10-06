#include "config.h"

// Only exists so the extern variables can be declared

int GRIDSIZESKYX = 64;
int GRIDSIZESKYY = 64;
int GRIDSIZESKYZ = 64;

int GRIDSIZESKY = (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ);
int GRIDSIZEGROUND = (GRIDSIZESKYX * GRIDSIZESKYZ);
float VOXELSIZE = 32.0f; // In Meters
