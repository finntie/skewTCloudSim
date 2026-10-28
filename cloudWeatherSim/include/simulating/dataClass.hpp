#pragma once
#include "config.hpp"
#include "environment.hpp"

#include <glm/glm.hpp>

struct microPhysEnvValues;

class dataClass
{
private:

	microPhysEnvValues microPhysEnvDataResult;

public:
	dataClass();
	~dataClass();

	void confirmMicroPhysCheckRegion(const glm::ivec3 minCorner, const glm::ivec3 maxCorner);
	void cancelMicroPhysCheckRegion();
	void setMicroPhysicsData(const microPhysEnvValuesSingle& params);

	void drawMicroPhysGraph(bool simActive);
	void addValueToGraph(const char* name, const float* value);
	void detailedLegendEntry(const char* label, const char* details,  const char* addsTo, const char* removesFrom, const char* extra = " ");

	bool microPhysCheckActive{ false};
	char dummy = 0, dummy2 = 0, dummy3 = 0;
	glm::ivec3 microPhysMinPos{ -1, -1, -1 };
	glm::ivec3 microPhysMaxPos{ -1, -1, -1 };
};


