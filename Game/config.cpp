#include "pch.h"
#include "config.h"

// Only exists so the extern variables can be declared

int GRIDSIZESKYX = 128;
int GRIDSIZESKYY = 128;
int GRIDSIZESKYZ = 128;

int GRIDSIZESKY = (GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ);
int GRIDSIZEGROUND = (GRIDSIZESKYX * GRIDSIZESKYZ);
float VOXELSIZE = 128.0f; // In Meters