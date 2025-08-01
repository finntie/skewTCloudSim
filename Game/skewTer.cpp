#include "skewTer.h"

#include "math/meteoformulas.h"
#include "math/constants.hpp"

#include "environment.h"
#include "core/engine.hpp"
#include "rendering/colors.hpp"
#include "rendering/debug_render.hpp"
#include "core/input.hpp"
#include "math/geometry.hpp"
#include <glm/glm.hpp>


void skewTer::setSkewT(skewTInfo skewT)
{
	m_skewT = skewT;
}

void skewTer::drawSkewT(const glm::vec2 pos, const float width, const float height)
{
	const float x = pos.x, y = pos.y;
	int xstart = m_skewT.startIdx;
	while (xstart++ < m_skewT.size && m_skewT.pressures[xstart] == 0);
	//TODO: width and height are broken

	for (float h = 0; h < 1000; h += 100)
	{
		glm::vec2 coords = convertToPlottingCoordinates(0, h, true, width, height);
		coords += pos;
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40 + x, coords.y, 0), glm::vec3(40 + x, coords.y, 0), bee::Colors::Grey);
	}
	for (float i = -100 + pos.x; i < 40 + pos.x; i += 10)
	{
		glm::vec2 coords = convertToPlottingCoordinates(i, 30000, false, width, height);
		coords;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(i, 0 + y, 0), glm::vec3(coords, 0), bee::Colors::Grey);
	}
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40 + x, 0 + y, 0), glm::vec3( 40 + x, 0 + y, 0), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(-40 + x, 0 + y, 0), glm::vec3(-40 + x, 100 + y, 0), bee::Colors::Black);

	//Temp and dewpoint
	{
		for (int i = 1; i < m_skewT.size; i++)
		{
			glm::vec2 tempCoords = convertToPlottingCoordinates(m_skewT.temps[i], m_skewT.pressures[i], true, width, height);
			glm::vec2 tempPrevCoords = convertToPlottingCoordinates(m_skewT.temps[i - 1], m_skewT.pressures[i - 1], true, width, height);
			tempCoords += pos;
			tempPrevCoords += pos;

			glm::vec2 dewCoords = convertToPlottingCoordinates(m_skewT.dewPoints[i], m_skewT.pressures[i], true, width, height);
			glm::vec2 dewPrevCoords = convertToPlottingCoordinates(m_skewT.dewPoints[i - 1], m_skewT.pressures[i - 1], true, width, height);
			dewCoords += pos;
			dewPrevCoords += pos;

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(tempCoords, 0.0f), glm::vec3(tempPrevCoords, 0.0f), bee::Colors::Red);
			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dewCoords, 0.0f), glm::vec3(dewPrevCoords, 0.0f), bee::Colors::Green);
		}
	}


	//Dry and moist adiabatics
	{
		std::unique_ptr<float[]> temps = std::make_unique<float[]>(m_skewT.size);

		//Dry adiabatic to LCL
		meteoformulas::getDryAdiabatic(m_skewT.temps[xstart], m_skewT.pressures[xstart], m_skewT.pressures, temps.get(), m_skewT.size);
		
		for (int j = xstart; j < m_skewT.size; j++)
		{
			//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
			glm::vec2 coords = convertToPlottingCoordinates(temps[j], m_skewT.pressures[j], true, width, height);
			glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], m_skewT.pressures[j - 1], true, width, height);
			coords += pos;
			coordsPrev += pos;
		
			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
		}

		//Moist adiabatic at LCL
		int offset = 0;
		glm::vec3 LCL = meteoformulas::getLCL(m_skewT.temps[xstart], m_skewT.pressures[xstart], xstart * VOXELSIZE, m_skewT.dewPoints[xstart]);
		meteoformulas::getMoistTemp(LCL.x, LCL.y, m_skewT.pressures, temps.get(), m_skewT.size, offset);
		if (offset != -1)
		{
			for (int j = offset + 1; j < m_skewT.size; j++)
			{
				//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
				glm::vec2 coords = convertToPlottingCoordinates(temps[j], m_skewT.pressures[j], true, width, height);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], m_skewT.pressures[j - 1], true, width, height);
				coords += pos;
				coordsPrev += pos;

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
			}
		}
	}
}

void skewTer::setVariable(skewTInfo::skewTParam param, const int i, const float value)
{
	switch (param)
	{
	case skewTer::skewTInfo::TEMP:
		m_skewT.temps[i] = value;
		break;
	case skewTer::skewTInfo::DP:
		m_skewT.dewPoints[i] = value;
		break;
	case skewTer::skewTInfo::P:
		m_skewT.pressures[i] = value;
		break;
	default:
		break;
	}
}

void skewTer::setArray(skewTInfo::skewTParam param, const float* input)
{
	switch (param)
	{
	case skewTer::skewTInfo::TEMP:
		std::memcpy(m_skewT.temps, input, m_skewT.size * sizeof(float));
		break;
	case skewTer::skewTInfo::DP:
		std::memcpy(m_skewT.dewPoints, input, m_skewT.size * sizeof(float));
		break;
	case skewTer::skewTInfo::P:
		std::memcpy(m_skewT.pressures, input, m_skewT.size * sizeof(float));
		break;
	default:
		break;
	}
}

void skewTer::setStartIdx(const int idx)
{
	m_skewT.startIdx = idx;
}

void skewTer::setAllArrays(float* T, float* D, float* Ps)
{
	std::memcpy(m_skewT.temps, T, m_skewT.size * sizeof(float));
	std::memcpy(m_skewT.dewPoints, D, m_skewT.size * sizeof(float));
	std::memcpy(m_skewT.pressures, Ps, m_skewT.size * sizeof(float));
}



glm::vec2 skewTer::convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float scaleWidth, const float maxHeight)
{
	//Respect hPa for height in meter using standard pressure
	float height = value;
	if (!pressure) height = meteoformulas::getStandardPressureAtHeight(0, value); //TODO: use standard height/pressure?


	//---------------------Log()----------------------

	height = (log10f(height) - log10f(100)) / (log10f(1000) - log10f(100)) * maxHeight;
	height = maxHeight - height;

	//-------------------------------------------------


	//Skew value
	float skewedTemp = temp + tanTheta * height;
	skewedTemp *= scaleWidth;

	return { skewedTemp, height };
}