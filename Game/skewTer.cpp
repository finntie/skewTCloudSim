#include "skewTer.h"

#include "math/meteoformulas.h"
#include "math/constants.hpp"

#include "environment.h"
#include "core/engine.hpp"
#include "rendering/colors.hpp"
#include "rendering/debug_render.hpp"
#include "platform/opengl/draw_image.hpp"

#include "core/input.hpp"
#include "math/geometry.hpp"
#include <glm/glm.hpp>


skewTer::skewTer()
{
	skewTImage = new bee::DrawImage();
}

skewTer::~skewTer()
{
	delete[] m_skewT.temps;
	delete[] m_skewT.dewPoints;
	delete[] m_skewT.pressures;
	if (skewTImage)
	{
		delete skewTImage;
	}
}

void skewTer::setSkewT(skewTInfo skewT)
{
	m_skewT = skewT;
}

void skewTer::drawSkewT()
{
	startHeight = m_skewT.startIdx;
	while (startHeight <= m_skewT.size && m_skewT.pressures[startHeight] == 0) { startHeight++; }
	startHeight = startHeight >= m_skewT.size ? m_skewT.size - 1 : startHeight;

	if (startHeight >= m_skewT.size - 1) return;

	drawBackground();
	drawEnvironment();
	drawDryAndMoist();
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

//-------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------               Drawing                     ------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------
void skewTer::drawBackground()
{
	const float scaleX = skewTSize.x / normalSkewTSize.x, scaleY = skewTSize.y / normalSkewTSize.y;

	for (float h = 0; h < 1000; h += 100)
	{
		glm::vec2 coords = convertToPlottingCoordinates(0, h, true, skewTSize.x, skewTSize.y);
		coords += skewTPos;
		skewTImage->AddLine(glm::vec3(-40 * scaleX + skewTPos.x, coords.y, 0), glm::vec3(40 * scaleX + skewTPos.x, coords.y, 0), bee::Colors::Grey);
	}
	for (float i = -100; i < 40; i += 10)
	{
		glm::vec2 coords = convertToPlottingCoordinates(i, 100, true, skewTSize.x, skewTSize.y);
		coords += skewTPos;

		skewTImage->AddLine(glm::vec3(i * scaleX + skewTPos.x, 0 + skewTPos.y, 0), glm::vec3(coords, 0), bee::Colors::Grey);
	}
	skewTImage->AddLine(glm::vec3(-40 * scaleX + skewTPos.x, 0 + skewTPos.y, 0), glm::vec3(40 * scaleX + skewTPos.x, 0 + skewTPos.y, 0), bee::Colors::Black);
	skewTImage->AddLine(glm::vec3(-40 * scaleX + skewTPos.x, 0 + skewTPos.y, 0), glm::vec3(-40 * scaleX + skewTPos.x, 100 * scaleY + skewTPos.y, 0), bee::Colors::Black);
}

void skewTer::drawEnvironment()
{
	//Temp and dewpoint
	{
		for (int i = startHeight + 1; i < m_skewT.size; i++)
		{
			glm::vec2 tempCoords = convertToPlottingCoordinates(m_skewT.temps[i], m_skewT.pressures[i], true, skewTSize.x, skewTSize.y);
			glm::vec2 tempPrevCoords = convertToPlottingCoordinates(m_skewT.temps[i - 1], m_skewT.pressures[i - 1], true, skewTSize.x, skewTSize.y);
			tempCoords += skewTPos;
			tempPrevCoords += skewTPos;

			glm::vec2 dewCoords = convertToPlottingCoordinates(m_skewT.dewPoints[i], m_skewT.pressures[i], true, skewTSize.x, skewTSize.y);
			glm::vec2 dewPrevCoords = convertToPlottingCoordinates(m_skewT.dewPoints[i - 1], m_skewT.pressures[i - 1], true, skewTSize.x, skewTSize.y);
			dewCoords += skewTPos;
			dewPrevCoords += skewTPos;

			skewTImage->AddLine(glm::vec3(tempCoords, 0.0f), glm::vec3(tempPrevCoords, 0.0f), bee::Colors::Red);
			skewTImage->AddLine(glm::vec3(dewCoords, 0.0f), glm::vec3(dewPrevCoords, 0.0f), bee::Colors::Green);
		}
	}
}

void skewTer::drawDryAndMoist()
{
	//Dry and moist adiabatics
	{
		std::unique_ptr<float[]> temps = std::make_unique<float[]>(m_skewT.size);

		//Dry adiabatic to LCL
		meteoformulas::getDryAdiabatic(m_skewT.temps[startHeight], m_skewT.pressures[startHeight], m_skewT.pressures, temps.get(), m_skewT.size);

		for (int j = startHeight + 1; j < m_skewT.size; j++)
		{
			//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
			glm::vec2 coords = convertToPlottingCoordinates(temps[j], m_skewT.pressures[j], true, skewTSize.x, skewTSize.y);
			glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], m_skewT.pressures[j - 1], true, skewTSize.x, skewTSize.y);
			coords += skewTPos;
			coordsPrev += skewTPos;

			skewTImage->AddLine(glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
		}

		//Moist adiabatic at LCL
		int offset = 0;
		glm::vec3 LCL = meteoformulas::getLCL(m_skewT.temps[startHeight], m_skewT.pressures[startHeight],0, m_skewT.dewPoints[startHeight]);
		meteoformulas::getMoistTemp(LCL.x, LCL.y, m_skewT.pressures, temps.get(), m_skewT.size, offset);
		if (offset != -1)
		{
			for (int j = offset + 1; j < m_skewT.size; j++)
			{
				//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
				glm::vec2 coords = convertToPlottingCoordinates(temps[j], m_skewT.pressures[j], true, skewTSize.x, skewTSize.y);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], m_skewT.pressures[j - 1], true, skewTSize.x, skewTSize.y);
				coords += skewTPos;
				coordsPrev += skewTPos;

				skewTImage->AddLine(glm::vec3(coords, 0), glm::vec3(coordsPrev, 0), bee::Colors::Black);
			}
		}
	}
}



glm::vec2 skewTer::convertToPlottingCoordinates(const float temp, const float value, const bool pressure, const float width, const float maxHeight)
{
	//Respect hPa for height in meter using standard pressure
	float height = value;
	if (!pressure) height = meteoformulas::getStandardPressureAtHeight(0, value); //Could use pressure from data.


	//---------------------Log()----------------------

	height = (log10f(height) - log10f(100)) / (log10f(1000) - log10f(100)) * maxHeight;
	height = maxHeight - height;

	//-------------------------------------------------


	//Skew value
	float skewedTemp = (temp * width / normalSkewTSize.x) + tanTheta * height;

	return { skewedTemp, height };
}