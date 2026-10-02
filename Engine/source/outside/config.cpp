#include "outside/config.h"

// Only exists so the extern variables can be declared

int GRIDSIZESKYX = 32;
int GRIDSIZESKYY = 32;
int GRIDSIZESKYZ = 32;

int GRIDSIZESKY = (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ);
int GRIDSIZEGROUND = (GRIDSIZESKYX * GRIDSIZESKYZ);
float VOXELSIZE = 32.0f; // In Meters
