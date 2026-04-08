#pragma once 
#include "config.h"
#include "glm/glm.hpp"

class tracing
{
public:

	enum showSettings
	{
		BELOW = 1, EQUAL = 2, ABOVE = 4
	};

	tracing();
	~tracing() {};

	void resetGrid(bool value = false);
	void showVoxelsWithValue(const float* array, const float value, showSettings settings);
	void setVoxelValue(const int index, const bool value);

	int getVoxelAtMouse();

	// Check if the ray will intersect the grid at all, using direction, origin and reciprocal direction (1/D)
	float intersectGrid(glm::vec3 dir, glm::vec3 origin, glm::vec3 recDir);

private:

	bool gridVoxels[GRIDSIZESKY];
	glm::vec3 gridMin{};
	glm::vec3 gridMax{};

};

