#include "config.h"

// Only exists so the extern variables can be declared

int GRIDSIZESKYX = 16;
int GRIDSIZESKYY = 16;
int GRIDSIZESKYZ = 16;

int GRIDSIZESKY = (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ);
int GRIDSIZEGROUND = (GRIDSIZESKYX * GRIDSIZESKYZ);
float VOXELSIZE = 64.0f; // In Meters
