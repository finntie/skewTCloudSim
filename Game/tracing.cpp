#include "tracing.h"

#include "math/geometry.hpp"

#include "core/engine.hpp"
#include "core/input.hpp"
#include "core/transform.hpp"

#include "rendering/render.hpp"

#include "tools/inspector.hpp"

#include "utils.cuh"

tracing::tracing()
{
	// Set grid bounds
	gridMin = glm::vec3(0, 0, 0);
	gridMax = gridMin + glm::vec3(GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ);
	resetGrid(true);
}

void tracing::resetGrid(bool value)
{
	std::fill_n(std::begin(gridVoxels), std::size(gridVoxels), value);
}

void tracing::showVoxelsWithValue(const float* array, const float value, showSettings settings)
{
	const bool below = settings & BELOW;
	const bool equal = settings & EQUAL;
	const bool above = settings & ABOVE;

	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		const float valueArray = array[i];
		if (below && valueArray < value)
		{
			gridVoxels[i] = true;
		}
		if (equal && valueArray == value)
		{
			gridVoxels[i] = true;
		}
		if (above && valueArray > value)
		{
			gridVoxels[i] = true;
		}
	}
}

void tracing::setVoxelValue(const int index, const bool value)
{
	gridVoxels[index] = value;
}

int tracing::getVoxelAtMouse()
{

	glm::vec3 O{};
	glm::vec3 D{};
	glm::vec3 rD{};

	//For each camera (we have 1)
	for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
	{
		O = transform.GetTranslation();
		D = bee::mouseRayDirection(bee::Engine.Input().GetMousePosition(), transform.GetRotation(), camera.Projection);
		// Safely set the reciprocal
		rD = glm::vec3(D.x == 0.0f ? 0.0f : 1.0f / D.x, D.y == 0.0f ? 0.0f : 1.0f / D.y, D.z == 0.0f ? 0.0f : 1.0f / D.z);
	}

	float t = 0.0f;

	// -------------------------------------------------------------------------------------
	// Code highly inspired from template IGAD version 3, IGAD/NHTV/UU - Jacco Bikker - 2006-2022
	// -------------------------------------------------------------------------------------

	// If ray is NOT in the grid
	if (!(O.x >= gridMin.x && O.x <= gridMax.x && O.y >= gridMin.y && O.y <= gridMax.y && O.z >= gridMin.z && O.z <= gridMax.z))
	{
		t = intersectGrid(D, O, rD);
		if (t > 1e33f) return -1; // Did not intersect grid at all
	}

	// Convert reversed direction into 0 or 1
	glm::vec3 stepSign = (glm::vec3(-copysign(1.0f, D.x), -copysign(1.0f, D.y), -copysign(1.0f, D.z)) + 1.0f) * 0.5f;
	// Step for direction with -1 meaning backwards and 1 forwards per axis.
	glm::ivec3 step = 1 - glm::ivec3(stepSign) * 2;
	const glm::vec3 posInGrid = (O + (t + 0.00005f) * D); // Position in grid
	const glm::vec3 gridPlanes = (glm::ceil(posInGrid) - stepSign); // Next boundary intersection we will intersect using ceil
	// Set starting position, making sure to clamp within grid
	glm::ivec3 pos = glm::ivec3(
		glm::clamp(int(posInGrid.x), 0, GRIDSIZESKYX - 1),
		glm::clamp(int(posInGrid.y), 0, GRIDSIZESKYY - 1),
		glm::clamp(int(posInGrid.z), 0, GRIDSIZESKYZ - 1));
	// How much to step to cross 1 full cell
	glm::vec3 tDelta = glm::vec3(step) * rD;
	glm::vec3 tMax = (gridPlanes - O) * rD; // Max distance to cross boundary of each axis


	// Start tracing through the grid
	while (1)
	{
		if (!isOutside(pos.x, pos.y, pos.z) && gridVoxels[getIdx(pos.x, pos.y, pos.z)])
		{
			return getIdx(pos.x, pos.y, pos.z);
		}
		else if (tMax.x < tMax.y && tMax.x < tMax.z) // If closest to x boundary
		{
			t = tMax.x;
			pos.x += step.x;
			if (pos.x >= GRIDSIZESKYX || pos.x < 0) return -1;
			tMax.x += tDelta.x;
		}
		else if (tMax.y < tMax.z) // y is closer than z
		{
			t = tMax.y;
			pos.y += step.y;
			if (pos.y >= GRIDSIZESKYY || pos.y < 0) return -1;
			tMax.y += tDelta.y;
		}
		else // z is closest
		{
			t = tMax.z;
			pos.z += step.z;
			if (pos.z >= GRIDSIZESKYZ || pos.z < 0) return -1;
			tMax.z += tDelta.z;
		}
	}

}

float tracing::intersectGrid(glm::vec3 dir, glm::vec3 origin, glm::vec3 recDir)
{
	// test if the ray intersects the cube

	glm::vec3 gridBounds[2] = { gridMin, gridMax };

	// Code from template IGAD version 3, IGAD/NHTV/UU - Jacco Bikker - 2006-2022
	const int signx = dir.x < 0, signy = dir.y < 0, signz = dir.z < 0;
	float tmin = (gridBounds[signx].x - origin.x) * recDir.x;
	float tmax = (gridBounds[1 - signx].x - origin.x) * recDir.x;
	const float tymin = (gridBounds[signy].y - origin.y) * recDir.y;
	const float tymax = (gridBounds[1 - signy].y - origin.y) * recDir.y;
	if (tmin > tymax || tymin > tmax) return 1e34f;
	tmin = std::max(tmin, tymin), tmax = std::min(tmax, tymax);
	const float tzmin = (gridBounds[signz].z - origin.z) * recDir.z;
	const float tzmax = (gridBounds[1 - signz].z - origin.z) * recDir.z;
	if (tmin > tzmax || tzmin > tmax) return 1e34f;
	if ((tmin = std::max(tmin, tzmin)) > 0) return tmin;	

	return 1e34f;
}
