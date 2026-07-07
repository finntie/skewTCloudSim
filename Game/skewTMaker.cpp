#include "pch.h"
#include "skewTMaker.h"

#include "math/meteoformulas.h"
#include "math/math.hpp"
#include <algorithm>
#include "readTable.h"
#include "glm/gtc/matrix_transform.hpp"
#include "math/constants.hpp"
#include "game.h"

void skewTMaker::init()
{
	// set center posses
	glm::vec2 centerPos = m_skewTPos + m_skewTSize * 0.5f;
	m_centerSkewTPos = glm::vec3(centerPos.x, 0.0f, centerPos.y);

	centerPos = m_skewTPos + m_skewTSize + glm::vec2(m_hodographOffset.x, -m_hodographOffset.y) + glm::vec2(m_windMax * m_hodoGraphDecrease, -m_windMax * m_hodoGraphDecrease);
	m_centerhodoPos = glm::vec3(centerPos.x, 0.0f, centerPos.y);

	// Set camera pos and rotation
	for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
	{
		transform.SetTranslation(m_centerSkewTPos + glm::vec3(0, 100, 0));
		transform.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(-1, 0, 0)));
	}
	glm::vec2 zeroCoord = convertToPlottingCoordinates(0, 1000, true);
	m_cursorPos = glm::vec3(zeroCoord.x, 0, -zeroCoord.y);


}

void skewTMaker::loadData(std::vector<float>& temp, std::vector<float>& dew, std::vector<float>& pressure, std::vector<float>& windSpeed, std::vector<glm::vec2>& windDir)
{
	m_simpleTemps.value = temp;
	m_simpleTemps.pressure = pressure;
	m_simpleDews.value = dew;
	m_simpleDews.pressure = pressure;
	m_simpleWindSpeed.value = windSpeed;
	m_simpleWindSpeed.pressure = pressure;
	m_windDir = windDir;


}

void skewTMaker::update(float dt)
{
	m_mousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());

	shortCuts();

	drawBackground();
	drawEnvironment();
	drawDryAndMoist();

	handleMouseInput();
	cameraControl(dt);

	resetControls();
}

void skewTMaker::panel()
{
	if (ImGui::Button("Confirm SkewT"))
	{
		confirmSkewT();
	}

	// Teleport to skew-T or back
	const char* hodographLabel = "Go to Hodograph";
	if (m_usingHodograph) hodographLabel = "Return to Skew-T";
	if (ImGui::Button(hodographLabel))
	{
		if (m_usingHodograph)
		{
			m_minZoom = 100.0f;
			m_zeroCamPos = m_centerSkewTPos + glm::vec3(0, m_minZoom, 0);
			m_currentParam = TEMPERATURE;
			m_cursorActive = false;
			m_editingPoint = -1;
			updateCursor(true);
		}
		else
		{
			m_minZoom = 50.0f;
			m_zeroCamPos = m_centerhodoPos + glm::vec3(0, m_minZoom, 0);
			m_currentParam = WIND;
			m_cursorActive = false;
			m_editingPoint = -1;
			updateCursor(true);
		}
		for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
		{
			transform.SetTranslation(glm::vec3(m_zeroCamPos.x, m_zeroCamPos.y, -
				m_zeroCamPos.z));
		}
		m_usingHodograph = !m_usingHodograph;
	}

	int param = int(m_currentParam);

	if (ImGui::RadioButton("Edit Temp", &param, 0)) 
	{
		m_currentParam = skewTParams(param);
		m_cursorActive = false;
		m_editingPoint = -1;
		updateCursor(true);
	}
	if (ImGui::RadioButton("Edit DewPoint", &param, 1))
	{
		m_currentParam = skewTParams(param);
		m_cursorActive = false;
		m_editingPoint = -1;
		updateCursor(true);
	}

	if (ImGui::Checkbox("Editing Mode", &m_editing))
	{
		m_cursorActive = false;
	}


	if (m_usingHodograph)
	{
		float heightInKM = meteoformulas::getStandardHeightAtPressure(0, m_currentWindHeight) / 1000.0f;
		if (ImGui::SliderFloat("Editing Wind at Height", &heightInKM, 0.0f, 16.0f))
		{
			m_currentWindHeight = meteoformulas::getStandardPressureAtHeight(0, heightInKM * 1000.0f);
		}
		if (ImGui::InputFloat("Standard Wind Height Increase in km", &m_standardIncreaseWind, 0.025f, 1.0f, "%.2f"))
		{
			m_standardIncreaseWind = std::max(m_standardIncreaseWind, 0.025f);
		}
	}


	tooltipsHelp();

}




void skewTMaker::handleMouseInput()
{

	if (m_editing)
	{
		updateCursor(false);

		if (m_cursorActive && m_editingPoint != -1)
		{
			visualizeAndConfirmCursorEditing();
		}

		selectUnselectCursor();

	}
	else
	{
		if (m_skewTConfirmed || m_skewTReady) return;

		// Set cursor and target pos with respect to boundaries
		updateCursor(false);

		if (m_cursorActive)
		{
			visualizeAndConfirmCursor();
		}

		undo();
		selectUnselectCursor();

		// Add circle at between points
		for (auto it : m_inBetweenPosses)
		{
			bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::General, it, m_cursorSize * (m_zoomValue + 0.2f), glm::vec3(0, 1, 0), bee::Colors::White);
		}
		m_inBetweenPosses.clear();

	}


	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::General, m_cursorPos, m_cursorSize * (m_zoomValue + 0.2f), glm::vec3(0, 1, 0), bee::Colors::White);
	if (m_cursorActive) bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::General, m_cursorPos, m_cursorSize * (m_zoomValue + 0.2f), glm::vec3(0, 1, 0), bee::Colors::White);
}

void skewTMaker::updateCursor(bool reset)
{
	if (m_editing && m_editingPoint != -1)
	{
		std::vector<float>* valuesVector = nullptr;
		if (m_currentParam == TEMPERATURE) valuesVector = &m_simpleTemps.pressure;
		else if (m_currentParam == DEWPOINT) valuesVector = &m_simpleDews.pressure;
		else return; // Nothing for wind yet

		if (!m_cursorActive)
		{
			glm::vec2 plotCoords{};

			if (m_currentParam == TEMPERATURE) plotCoords = convertToPlottingCoordinates(m_simpleTemps.value[m_editingPoint], m_simpleTemps.pressure[m_editingPoint], true);
			else if (m_currentParam == DEWPOINT) plotCoords = convertToPlottingCoordinates(m_simpleDews.value[m_editingPoint], m_simpleDews.pressure[m_editingPoint], true);
			else return; // Nothing for wind yet

			m_cursorPos.x = plotCoords.x;
			m_cursorPos.z = -plotCoords.y;
			m_targetPos = m_cursorPos;
		}
		else
		{
			// Clamp with upper value and lower value
			float upperValue = 0.0f, lowerValue = 0.0f;

			if (m_editingPoint >= valuesVector->size() - 1) upperValue = m_cursorPos.z - m_maxHeightAdd;
			else
			{
				glm::vec2 plotCoords{};
				if (m_currentParam == TEMPERATURE) plotCoords = convertToPlottingCoordinates(m_simpleTemps.value[m_editingPoint + 1], m_simpleTemps.pressure[m_editingPoint + 1], true);
				else if (m_currentParam == DEWPOINT) plotCoords = convertToPlottingCoordinates(m_simpleDews.value[m_editingPoint + 1], m_simpleDews.pressure[m_editingPoint + 1], true);
				else return; // Nothing for wind yet
				upperValue = -plotCoords.y + m_mininumHeightAdd;
			}
			if (m_editingPoint == 0) lowerValue = m_cursorPos.z, upperValue = m_cursorPos.z; // If at first point, we dont allow change in height
			else
			{
				glm::vec2 plotCoords{};
				if (m_currentParam == TEMPERATURE) plotCoords = convertToPlottingCoordinates(m_simpleTemps.value[m_editingPoint - 1], m_simpleTemps.pressure[m_editingPoint - 1], true);
				else if (m_currentParam == DEWPOINT) plotCoords = convertToPlottingCoordinates(m_simpleDews.value[m_editingPoint - 1], m_simpleDews.pressure[m_editingPoint - 1], true);
				else return; // Nothing for wind yet
				lowerValue = -plotCoords.y - m_mininumHeightAdd;
			}

			m_targetPos.x = m_mousePos3D.x;
			m_targetPos.z = upperValue >= lowerValue ? m_targetPos.z : std::clamp(m_mousePos3D.z, upperValue, lowerValue);

			if (m_currentParam == TEMPERATURE) checkEdgeCasesTemp();
			else if (m_currentParam == DEWPOINT) checkEdgeCasesDew();
			else return; // Nothing for wind yet
		}
	}
	else
	{
		if (m_cursorActive || reset)
		{
			switch (m_currentParam)
			{
			case skewTMaker::TEMPERATURE:

				if (m_simpleTemps.value.empty())
				{
					// Set cursor when at empty temp or dew
					float dewTemp = m_simpleDews.value.empty() ? 0.0f : m_simpleDews.value.front();
					glm::vec2 plotCoord = convertToPlottingCoordinates(dewTemp, 1000, true);
					plotCoord.x += 0.05f;
					m_targetPos.x = m_cursorPos.x = reset ? plotCoord.x : std::max(plotCoord.x, m_mousePos3D.x);
					m_cursorPos.z = -plotCoord.y;
					m_targetPos.z = m_cursorPos.z;
				}
				else
				{
					m_targetPos = m_mousePos3D;
					glm::vec2 plotCoords = convertToPlottingCoordinates(m_simpleTemps.value[int(m_simpleTemps.value.size()) - 1], m_simpleTemps.pressure[int(m_simpleTemps.pressure.size()) - 1], true);
					m_cursorPos.x = plotCoords.x;
					m_cursorPos.z = -plotCoords.y;
					m_targetPos.z = std::clamp(m_targetPos.z, m_cursorPos.z - m_maxHeightAdd, m_cursorPos.z - m_mininumHeightAdd); // Make sure we always go up by at least 1 (not downwards) and maximal upwards by 5
					m_targetPos.z = std::max(m_targetPos.z, -(m_skewTPos.y + m_skewTSize.y));
				}

				if (!reset) checkEdgeCasesTemp();

				break;
			case skewTMaker::DEWPOINT:

				if (m_simpleDews.value.empty())
				{
					// Set cursor when at empty temp or dew
					float temp = m_simpleTemps.value.empty() ? 0.0f : m_simpleTemps.value.front();
					glm::vec2 plotCoord = convertToPlottingCoordinates(temp, 1000, true);
					plotCoord.x -= 0.05f;
					m_targetPos.x = m_cursorPos.x = reset ? plotCoord.x : std::min(plotCoord.x, m_mousePos3D.x);
					m_cursorPos.z = -plotCoord.y;
					m_targetPos.z = m_cursorPos.z;
				}
				else
				{
					m_targetPos = m_mousePos3D;
					glm::vec2 plotCoords = convertToPlottingCoordinates(m_simpleDews.value[int(m_simpleDews.value.size()) - 1], m_simpleDews.pressure[int(m_simpleDews.pressure.size()) - 1], true);
					m_cursorPos.x = plotCoords.x;
					m_cursorPos.z = -plotCoords.y;
					m_targetPos.z = std::clamp(m_targetPos.z, m_cursorPos.z - m_maxHeightAdd, m_cursorPos.z - m_mininumHeightAdd); // Make sure we always go up by at least 1 (not downwards) and maximal upwards by 5
					m_targetPos.z = std::max(m_targetPos.z, -(m_skewTPos.y + m_skewTSize.y));
				}

				if (!reset) checkEdgeCasesDew();

				break;
			case skewTMaker::WIND:
			{
				// Get distance and direction to mousepos
				glm::vec3 correctCenter = glm::vec3(m_centerhodoPos.x, m_centerhodoPos.y, -m_centerhodoPos.z);
				float dist = glm::distance(m_mousePos3D, correctCenter);
				glm::vec3 dir = glm::normalize(m_mousePos3D - correctCenter);
				// P = O + D * T
				glm::vec3 maxPos = correctCenter + dir * m_windMax * m_hodoGraphDecrease;
				bool shouldClamp = dist > m_windMax * m_hodoGraphDecrease;

				if (m_simpleWindSpeed.value.empty())
				{
					m_targetPos = reset ? glm::vec3(m_centerhodoPos.x, m_centerhodoPos.y, -m_centerhodoPos.z) : (shouldClamp ? maxPos : m_mousePos3D);
					m_cursorPos = m_targetPos;
				}
				else
				{
					m_targetPos = shouldClamp ? maxPos : m_mousePos3D;
					m_cursorPos = convertWindToCoordinates(m_simpleWindSpeed.value.back(), m_windDir.back());
				}
			}
				break;
			default:
				break;
			}
		}
	}
}

void skewTMaker::checkEdgeCasesTemp()
{
	// Check all edge cases which might happen and need to be handled accordingly.

	glm::vec2 targetData;
	glm::vec2 currentData;

	// Set the target data correctly
	if (m_editingPoint == -1)
	{
		convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), targetData);
		convertToTempAndPressure(glm::vec2(m_cursorPos.x, m_cursorPos.z), currentData);
	}
	else
	{
		// Use point
		convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), targetData);
		currentData = glm::vec2(m_simpleTemps.value[0], m_simpleTemps.pressure[0]);
	}

	std::vector<int> inBetweenPoints;

	bool valueIsTarget1 = false, foundAroundTarget = false, foundInBetweenTargets = false;
	std::vector<int> pointsCloseTo;

	if (!getAllIndicesAround(m_simpleDews.pressure, valueIsTarget1, foundAroundTarget, foundInBetweenTargets, pointsCloseTo, targetData.y, currentData.y))
	{
		// Failed or just not things to be checked
		return;
	}

	if (m_editing)
	{
		if (m_cursorActive && (foundAroundTarget || foundInBetweenTargets))
		{
			// We have to make sure our temp point can not go below the dew line in between the two possible targets we found

			if (foundAroundTarget)
			{
				// When in between 2 dewpoint lines, we have to make sure our current editing point does not cross the line

				if (valueIsTarget1)
				{
					// We only need to check if current point is colder than dewpoint point
					targetData.x = std::max(targetData.x, m_simpleDews.value[pointsCloseTo.back()] + m_offset);
				}
				else
				{
					// We know the last two values are our points of interest
					// Lerp to get temp in between, we log due to pressure being in log scale while temp is not
					float logPBack = std::log(m_simpleDews.pressure[pointsCloseTo.back()]);
					float logPPrev = std::log(m_simpleDews.pressure[pointsCloseTo.back() - 1]);
					float logPTarget = std::log(targetData.y);
					float r = (logPBack - logPTarget) / (logPBack - logPPrev);
					float tempAtr = bee::Lerp(m_simpleDews.value[pointsCloseTo.back()], m_simpleDews.value[pointsCloseTo.back() - 1], r);
					targetData.x = std::max(targetData.x, tempAtr + m_offset);

				}
			}

			// Now we have to check if our lines above and below intersect with any dewpoint point. 
			// This we do by first grabbing all dewpoint points in between previous and next temperature point

			glm::vec2 target1 = m_editingPoint >= m_simpleTemps.pressure.size() - 1 ? targetData: glm::vec2(m_simpleTemps.value[m_editingPoint + 1], m_simpleTemps.pressure[m_editingPoint + 1]);
			glm::vec2 target2 = m_editingPoint <= 0 ? targetData : glm::vec2(m_simpleTemps.value[m_editingPoint - 1], m_simpleTemps.pressure[m_editingPoint - 1]);

			// Check if we are not setting current point on top of another point
			const float current = m_simpleTemps.pressure[m_editingPoint];
			if (m_editingPoint < m_simpleTemps.pressure.size() - 1) targetData.y = std::max(current, target1.y + 0.005f); // Current must be greater than next index
			if (m_editingPoint > 0) targetData.y = std::min(current, target2.y - 0.005f); // Current must be smaller than previous index	

			pointsCloseTo.clear();
			if (getAllIndicesAround(m_simpleDews.pressure, valueIsTarget1, foundAroundTarget, foundInBetweenTargets, pointsCloseTo, target1.y, target2.y))
			{
				bool endedAtOtherSide{ false };
				glm::vec2 iPoint{ 0,0 };
				fillInBetweens(pointsCloseTo, m_simpleTemps, m_simpleDews, target2, targetData, endedAtOtherSide, iPoint);
				fillInBetweens(pointsCloseTo, m_simpleTemps, m_simpleDews, targetData, target1, endedAtOtherSide, iPoint);
			}
		}
	}
	else
	{
		if (foundAroundTarget || foundInBetweenTargets)
		{
			// Check for points in between and add them to an array
			bool endedAtOtherSide{ false };
			glm::vec2 iPoint{ 0,0 };
			fillInBetweens(pointsCloseTo, m_simpleTemps, m_simpleDews, currentData, targetData, endedAtOtherSide, iPoint);

			// Check if our target ended up in the cold side and intersected
			if (endedAtOtherSide)
			{
				targetData = iPoint;
				targetData.x += m_offset;
			}
		}
	}

	glm::vec2 newTarget = convertToPlottingCoordinates(targetData.x, targetData.y, true);
	m_targetPos = glm::vec3(newTarget.x, 0.0f, -newTarget.y);
}

void skewTMaker::checkEdgeCasesDew()
{
	// Check all edge cases which might happen and need to be handled accordingly.

	glm::vec2 targetData;
	glm::vec2 currentData;

	// Set the target data correctly
	if (m_editingPoint == -1)
	{
		convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), targetData);
		convertToTempAndPressure(glm::vec2(m_cursorPos.x, m_cursorPos.z), currentData);
	}
	else
	{
		// Use point
		convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), targetData);
		currentData = glm::vec2(m_simpleDews.value[0], m_simpleDews.pressure[0]);
	}

	std::vector<int> inBetweenPoints;

	bool valueIsTarget1 = false, foundAroundTarget = false, foundInBetweenTargets = false;
	std::vector<int> pointsCloseTo;

	if (!getAllIndicesAround(m_simpleTemps.pressure, valueIsTarget1, foundAroundTarget, foundInBetweenTargets, pointsCloseTo, targetData.y, currentData.y))
	{
		// Failed or just not things to be checked
		return;
	}

	if (m_editing)
	{
		if (m_cursorActive && (foundAroundTarget || foundInBetweenTargets))
		{
			// We have to make sure our temp point can not go below the dew line in between the two possible targets we found

			if (foundAroundTarget)
			{
				// When in between 2 dewpoint lines, we have to make sure our current editing point does not cross the line

				if (valueIsTarget1)
				{
					// We only need to check if current point is colder than dewpoint point
					targetData.x = std::min(targetData.x, m_simpleTemps.value[pointsCloseTo.back()] - m_offset);
				}
				else
				{
					// We know the last two values are our points of interest
					// Lerp to get temp in between, we log due to pressure being in log scale while temp is not
					float logPBack = std::log(m_simpleTemps.pressure[pointsCloseTo.back()]);
					float logPPrev = std::log(m_simpleTemps.pressure[pointsCloseTo.back() - 1]);
					float logPTarget = std::log(targetData.y);
					float r = (logPBack - logPTarget) / (logPBack - logPPrev);
					float tempAtr = bee::Lerp(m_simpleTemps.value[pointsCloseTo.back()], m_simpleTemps.value[pointsCloseTo.back() - 1], r);
					targetData.x = std::min(targetData.x, tempAtr - m_offset);
				}
			}

			// Now we have to check if our lines above and below intersect with any dewpoint point. 
			// This we do by first grabbing all dewpoint points in between previous and next temperature point

			glm::vec2 target1 = m_editingPoint >= m_simpleDews.pressure.size() - 1 ? targetData : glm::vec2(m_simpleDews.value[m_editingPoint + 1], m_simpleDews.pressure[m_editingPoint + 1]);
			glm::vec2 target2 = m_editingPoint <= 0 ? targetData : glm::vec2(m_simpleDews.value[m_editingPoint - 1], m_simpleDews.pressure[m_editingPoint - 1]);

			// Check if we are not setting current point on top of another point
			const float current = m_simpleDews.pressure[m_editingPoint];
			if (m_editingPoint < m_simpleDews.pressure.size() - 1) targetData.y = std::max(current, target1.y + 0.005f); // Current must be greater than next index
			if (m_editingPoint > 0) targetData.y = std::min(current, target2.y - 0.005f); // Current must be smaller than previous index	

			pointsCloseTo.clear();
			if (getAllIndicesAround(m_simpleTemps.pressure, valueIsTarget1, foundAroundTarget, foundInBetweenTargets, pointsCloseTo, target1.y, target2.y))
			{
				bool endedAtOtherSide{ false };
				glm::vec2 iPoint{ 0,0 };
				fillInBetweens(pointsCloseTo, m_simpleDews, m_simpleTemps, target2, targetData, endedAtOtherSide, iPoint);
				fillInBetweens(pointsCloseTo, m_simpleDews, m_simpleTemps, targetData, target1, endedAtOtherSide, iPoint);
			}
		}
	}
	else
	{
		if (foundAroundTarget || foundInBetweenTargets)
		{
			// Check for points in between and add them to an array
			bool endedAtOtherSide{ false };
			glm::vec2 iPoint{ 0,0 };
			fillInBetweens(pointsCloseTo, m_simpleDews, m_simpleTemps, currentData, targetData, endedAtOtherSide, iPoint);

			// Check if our target ended up in the cold side and intersected
			if (endedAtOtherSide)
			{
				targetData = iPoint;
				targetData.x -= m_offset;
			}
		}
	}

	glm::vec2 newTarget = convertToPlottingCoordinates(targetData.x, targetData.y, true);
	m_targetPos = glm::vec3(newTarget.x, 0.0f, -newTarget.y);
}

void skewTMaker::fillInBetweens(std::vector<int>& posses, simpleData& , simpleData& checkingData, glm::vec2 currentData, glm::vec2 targetData, bool& endedInOtherSector, glm::vec2& iPoint)
{
	bool targetDew = &checkingData == &m_simpleTemps;
	bool otherSide = false;
	std::vector<int> inBetweenPoints;
	std::vector<int> idxsAround;
	bool saveIntersect = false;
	for (auto it : posses)
	{
		// For pressure we log to get the correct coordinate system
		glm::vec2 P1T = inBetweenPoints.empty() ? glm::vec2(currentData.x, std::log(currentData.y)) : glm::vec2(checkingData.value[inBetweenPoints.back()], std::log(checkingData.pressure[inBetweenPoints.back()]));
		glm::vec2 P2T = glm::vec2(targetData.x, std::log(targetData.y));

		glm::vec2 P1D = glm::vec2(checkingData.value[it], std::log(checkingData.pressure[it]));
		glm::vec2 P2D = glm::vec2(checkingData.value[it - 1], std::log(checkingData.pressure[it - 1]));
		glm::vec2 tempPoint{ 0,0 };
		bool intersected = intersectionPoint(P1T, P2T, P1D, P2D, tempPoint);
		// We can now 'unlog' the pressure
		tempPoint.y = std::exp(tempPoint.y);

		if (intersected)
		{
			saveIntersect = true;
			iPoint = tempPoint;
			if (otherSide) // Intersecting with line going into warm side
			{
				for (auto itArounds : idxsAround)
				{
					inBetweenPoints.push_back(itArounds);
				}
				idxsAround.clear();
				inBetweenPoints.push_back(it - 1);

				otherSide = false;
			}
			else // Intersecting with line going into cold side
			{
				otherSide = true;
			}
		}
		else if (otherSide) // If not intersecting with line in cold side
		{
			// Mark point as possible arounder
			idxsAround.push_back(it - 1);
		}
	}

	// If lowest pressure is still bigger than our lowest target this means that the last index was in between, meaning we can add an in-between point at the last index
	if (checkingData.pressure[posses.back()] > targetData.y && otherSide && saveIntersect)
	{
		for (auto itArounds : idxsAround)
		{
			inBetweenPoints.push_back(itArounds);
		}
		idxsAround.clear();
		inBetweenPoints.push_back(posses.back());
		otherSide = false;
	}

	// Transform between points into positions
	for (auto it : inBetweenPoints)
	{
		glm::vec2 pos = convertToPlottingCoordinates(checkingData.value[it], checkingData.pressure[it], true);
		float offset = targetDew ? -m_offset : m_offset;
		m_inBetweenPosses.push_back(glm::vec3(pos.x + offset, 0.0f, -pos.y));
	}

	endedInOtherSector = otherSide && saveIntersect;
}

void skewTMaker::visualizeAndConfirmCursor()
{
	glm::vec2 tempAt = glm::vec2(0);
	glm::vec2 windDir = glm::vec2(0);
	float windspeed = 0.0f;

	// If valid position
	if (m_currentParam != WIND)
	{
		if (!convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), tempAt) && m_currentParam != WIND) return; // Unable to convert to temp and presssure
	}
	else
	{
		convertCoordinatesToWind(m_targetPos, windspeed, windDir);
	}

	// First visualize 

	glm::vec4 color{};
	switch (m_currentParam)
	{
	case skewTMaker::TEMPERATURE: color = glm::vec4(0.5f, 0, 0, 1); break;
	case skewTMaker::DEWPOINT:color = glm::vec4(0, 0.5f, 0, 1); break;
	case skewTMaker::WIND: color = glm::vec4(0.5f, 1, 1, 1); break;
	default: break;
	}

	// Loop over all in between points if they are there
	if (!m_inBetweenPosses.empty())
	{
		glm::vec3 prevIt = m_cursorPos;
		for (auto it : m_inBetweenPosses)
		{
			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, prevIt, it, color);
			prevIt = it;
		}
		// Also one last line to targetpos
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, prevIt, m_targetPos, color);
	}
	else
	{
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, m_cursorPos, m_targetPos, color);
	}


	// Confirming



	if (lastValidChecks() && bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
	{
		simpleData& dataUsed = (m_currentParam == skewTMaker::TEMPERATURE) ? m_simpleTemps : ((m_currentParam == skewTMaker::DEWPOINT) ? m_simpleDews : m_simpleWindSpeed);

		switch (m_currentParam)
		{
		case skewTMaker::TEMPERATURE:
		case skewTMaker::DEWPOINT:
			for (auto it : m_inBetweenPosses)
			{
				glm::vec2 tempAtPos = glm::vec2(0);
				convertToTempAndPressure(glm::vec2(it.x, it.z), tempAtPos);
				dataUsed.value.push_back(tempAtPos.x);
				dataUsed.pressure.push_back(tempAtPos.y);
			}
			dataUsed.value.push_back(tempAt.x);
			dataUsed.pressure.push_back(tempAt.y);
			break;
		case skewTMaker::WIND:
		{
			dataUsed.value.push_back(windspeed);
			dataUsed.pressure.push_back(m_currentWindHeight);
			m_windDir.push_back(windDir);
			// Increase current wind height
			float H0 = meteoformulas::getStandardHeightAtPressure(0, m_currentWindHeight) / 1000.0f;
			H0 += m_standardIncreaseWind;
			m_currentWindHeight = H0 <= 16.0f ? meteoformulas::getStandardPressureAtHeight(0, H0 * 1000.0f) : m_currentWindHeight;
		}
			break;
		default: break;
		}


		// Check if skewT is valid
		if (m_targetPos.z == -(m_skewTPos.y + m_skewTSize.y))
		{
			m_cursorActive = false;
			if (m_currentParam == TEMPERATURE) m_currentParam = DEWPOINT, updateCursor(true);
			else if (m_currentParam == DEWPOINT) m_currentParam = TEMPERATURE, updateCursor(true);
			if (m_simpleTemps.pressure.size() == m_simpleDews.pressure.size()) m_skewTReady = true;
		}
	}

}

void skewTMaker::visualizeAndConfirmCursorEditing()
{
	glm::vec2 tempAt = glm::vec2(0);

	// If valid position
	if (convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), tempAt))
	{
		// First visualize 

		glm::vec4 color{};
		switch (m_currentParam)
		{
		case skewTMaker::TEMPERATURE: color = glm::vec4(0.8f, 0, 0, 0.7f); break;
		case skewTMaker::DEWPOINT: color = glm::vec4(0, 0.8f, 0, 0.7f); break;
		case skewTMaker::WIND: color = glm::vec4(0.8f, 1, 1, 0.7f); break;
		default: break;
		}

		glm::vec2 targetData;
		glm::vec2 currentData;

		// Set the target data correctly
		convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), targetData);
		convertToTempAndPressure(glm::vec2(m_cursorPos.x, m_cursorPos.z), currentData);

		simpleData& dataUsed = (m_currentParam == skewTMaker::TEMPERATURE) ? m_simpleTemps : m_simpleDews;

		// Loop over all in between points if they are there
		if (!m_inBetweenPosses.empty())
		{
			glm::vec3 prevIt = m_cursorPos;
			if (m_editingPoint > 0) // Use previous point if possible
			{
				glm::vec2 pos = convertToPlottingCoordinates(dataUsed.value[m_editingPoint - 1], dataUsed.pressure[m_editingPoint - 1], true);
				prevIt = glm::vec3(pos.x, 0.0f, -pos.y);
			}

			for (auto it : m_inBetweenPosses)
			{
				// Don't forget, reversed Z coords, we check if previous iterator is beneath target and next one is above it.
				// If this is the case, we need to insert the targetpos.
				if (m_targetPos.z <= prevIt.z && m_targetPos.z > it.z)
				{
					bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, prevIt, m_targetPos, color);
					bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, m_targetPos, it, color);
				}
				else bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, prevIt, it, color);
				prevIt = it;
			}
			// If in between posses were all before the targetpos, we still have to add a line to the targetpos
			if (prevIt.z > m_targetPos.z)
			{
				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, prevIt, m_targetPos, color);
				prevIt = m_targetPos;
			}

			if (m_editingPoint < dataUsed.pressure.size() - 1) // Color to next point if possible
			{
				glm::vec2 pos = convertToPlottingCoordinates(dataUsed.value[m_editingPoint + 1], dataUsed.pressure[m_editingPoint + 1], true);
				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, prevIt, glm::vec3(pos.x, 0.0f, -pos.y), color);
			}
		}
		else
		{
			if (m_editingPoint > 0) // Use previous point if possible
			{
				glm::vec2 pos = convertToPlottingCoordinates(dataUsed.value[m_editingPoint - 1], dataUsed.pressure[m_editingPoint - 1], true);
				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(pos.x, 0.0f, -pos.y), m_targetPos, color);
			}
			if (m_editingPoint < dataUsed.pressure.size() - 1)
			{
				glm::vec2 pos = convertToPlottingCoordinates(dataUsed.value[m_editingPoint + 1], dataUsed.pressure[m_editingPoint + 1], true);
				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, m_targetPos, glm::vec3(pos.x, 0.0f, -pos.y), color);
			}
		}
	}

	// Confirming


	if (lastValidChecks() && !bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
	{

		simpleData& dataUsed = (m_currentParam == skewTMaker::TEMPERATURE) ? m_simpleTemps : m_simpleDews;

		// Update target point itself
		glm::vec2 tempAtPos = glm::vec2(0);
		convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), tempAtPos);

		// Handle target cursor
		dataUsed.value[m_editingPoint] = tempAtPos.x;
		dataUsed.pressure[m_editingPoint] = tempAtPos.y;

		// Add all in between points
		int atIdx = m_editingPoint;
		for (auto it : m_inBetweenPosses)
		{
			if (it.z < m_targetPos.z) // If above target, we want to add posses after the target
			{
				atIdx++;
			}

			tempAtPos = glm::vec2(0);
			convertToTempAndPressure(glm::vec2(it.x, it.z), tempAtPos);

			dataUsed.value.insert(dataUsed.value.begin() + atIdx, tempAtPos.x);
			dataUsed.pressure.insert(dataUsed.pressure.begin() + atIdx, tempAtPos.y);
			atIdx++;
		}

		m_editingPoint = -1;
	}
	m_inBetweenPosses.clear();
}

void skewTMaker::selectUnselectCursor()
{
	// If cursor is near
	m_cursorSize = 0.5f;
	if (!m_cursorActive && distance(m_mousePos3D, m_cursorPos) <= 1.5f)
	{
		m_cursorSize = 0.6f;
		if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
		{
			m_cursorActive = true;
			m_cursorSize = 0.4f;

			glm::vec2 targetData{};
			convertToTempAndPressure(glm::vec2(m_mousePos3D.x, m_mousePos3D.z), targetData);
			bool foundValue = false;
			std::vector<float>* valuesVector = nullptr;
			if (m_currentParam == TEMPERATURE) valuesVector = &m_simpleTemps.pressure;
			else if (m_currentParam == DEWPOINT) valuesVector = &m_simpleDews.pressure;
			else return; // Nothing for wind yet
			// Get closest index to mouse cursor
			if (valuesVector && !getNearestIndex(*valuesVector, foundValue, m_editingPoint, targetData.y)) return;
		}
	}
	else if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Right))
	{
		if (m_cursorActive) m_cursorActive = false;
	}

	if (m_editing)
	{
		if (!m_cursorActive)
		{
			// Set editing point if editing
			m_editingPoint = -1;

			glm::vec2 targetData{};
			convertToTempAndPressure(glm::vec2(m_mousePos3D.x, m_mousePos3D.z), targetData);

			bool foundValue = false;
			std::vector<float>* valuesVector = nullptr;
			if (m_currentParam == TEMPERATURE) valuesVector = &m_simpleTemps.pressure;
			else if (m_currentParam == DEWPOINT) valuesVector = &m_simpleDews.pressure;
			else return; // Nothing for wind yet

			// Get closest index to mouse cursor
			if (valuesVector && !getNearestIndex(*valuesVector, foundValue, m_editingPoint, targetData.y)) return;
			else if (!foundValue) return;
		}
		else
		{
			if (!bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left) && !bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
			{
				// Force active to false
				m_cursorActive = false;
			}
		}
	}
	else
	{
		m_editingPoint = -1;
	}
}

void skewTMaker::undo()
{
	if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Right))
	{
		if (!m_cursorActive)
		{
			// Undo previous 
			switch (m_currentParam)
			{
			case skewTMaker::TEMPERATURE:
				if (!m_simpleTemps.pressure.empty())
				{
					m_simpleTemps.value.pop_back();
					m_simpleTemps.pressure.pop_back();
					updateCursor(true);
				}
				break;
			case skewTMaker::DEWPOINT:
				if (!m_simpleDews.pressure.empty())
				{
					m_simpleDews.value.pop_back();
					m_simpleDews.pressure.pop_back();
					updateCursor(true);
				}
				break;
			case skewTMaker::WIND:
				if (!m_simpleWindSpeed.pressure.empty())
				{
					m_simpleWindSpeed.value.pop_back();
					m_simpleWindSpeed.pressure.pop_back();
					m_windDir.pop_back();
					float H0 = meteoformulas::getStandardHeightAtPressure(0, m_currentWindHeight) / 1000.0f;
					H0 -= m_standardIncreaseWind;
					m_currentWindHeight = H0 > 0.0f ? meteoformulas::getStandardPressureAtHeight(0, H0 * 1000.0f) : m_currentWindHeight;
					updateCursor(true);
				}
				break;
			default:
				break;
			}
		}
	}
}

bool skewTMaker::lastValidChecks()
{
	bool valid = true;


	if (!m_editing)
	{
		switch (m_currentParam)
		{
		case skewTMaker::TEMPERATURE: if (!m_simpleTemps.value.empty() && m_cursorPos.z - m_targetPos.z < m_mininumHeightAdd) valid = false; break;
		case skewTMaker::DEWPOINT: if (!m_simpleDews.value.empty() && m_cursorPos.z - m_targetPos.z < m_mininumHeightAdd) valid = false; break;
		case skewTMaker::WIND: if (!m_simpleWindSpeed.pressure.empty() && m_simpleWindSpeed.pressure.back() == m_currentWindHeight) valid = false; break;
		default: break;
		}
	}

	if (m_editing && m_editingPoint != -1)
	{
		// Check if we are not setting current point on top of another point
		simpleData& dataUsed = (m_currentParam == skewTMaker::TEMPERATURE) ? m_simpleTemps : ((m_currentParam == skewTMaker::DEWPOINT) ? m_simpleDews : m_simpleWindSpeed);
		const float current = dataUsed.pressure[m_editingPoint];
		const float previous = m_editingPoint == 0 ? current + 1.0f : dataUsed.pressure[m_editingPoint - 1];
		const float next = m_editingPoint == dataUsed.pressure.size() - 1 ? current - 1.0f : dataUsed.pressure[m_editingPoint + 1];
		valid = previous >= current && next <= current; // Pressure of previous must be greater and pressure of next must be smaller than current
	}

	return valid;
}

bool skewTMaker::getNearestIndex(std::vector<float> values, bool& foundNearest, int& closestPoint, float target)
{
	foundNearest = false;
	if (values.empty()) return false;

	float closestDistance = 1e30f;
	for (int i = 0; i < int(values.size()); i++)
	{
		const float distance = abs(target - values[i]);
		if (distance < closestDistance)
		{
			closestDistance = distance;
			closestPoint = i;
			foundNearest = true;
		}
	}

	return true;
}

bool skewTMaker::getNearbyIndices(std::vector<float> values, bool& valueIsTarget, bool& foundAroundTarget, bool& foundInBetweenTargets, std::vector<int>& outputIndices, float target1, float target2)
{
	valueIsTarget = false; 
	foundAroundTarget = false; 
	foundInBetweenTargets = false;

	if (values.empty() || target2 <= target1) return false;


	// First get iterator to value lower than target1
	auto lowerTarget1 = std::upper_bound(values.begin(), values.end(), target1, std::greater<float>());

	// Lower is same as target  (also means that this was the lowest value in the vector)
	if (lowerTarget1 != values.end() && *lowerTarget1 == target1)
	{
		foundAroundTarget = false;
		valueIsTarget = true;
		outputIndices.push_back(int(lowerTarget1 - values.begin()));
	}
	// Check if we even found a value that is lower than target
	else
	{
		valueIsTarget = false;
		if (lowerTarget1 == values.end()) foundAroundTarget = false;
		else
		{
			outputIndices.push_back(int(lowerTarget1 - values.begin()));
			// Push back second value to get surrounding if we were not at the begin already
			if (lowerTarget1 != values.begin())
			{
				outputIndices.push_back(int(lowerTarget1 - values.begin() - 1));
				foundAroundTarget = true;
			}
			else return true; // There is nothing else to do
		}
	}

	// Return if there is no second target set
	if (target2 <= 0.0f)
	{
		foundInBetweenTargets = false;
		return true;
	}

	// Get value lower than target 2
	auto lowerTarget2 = std::upper_bound(values.begin(), values.end(), target2, std::greater<float>());

	if (lowerTarget1 == lowerTarget2)
	{
		// If values are the same but foundAroundTarget is true, it means that we are done
		// Else we know that value has to be target1, so there is nothing in between meaning we are done
		foundInBetweenTargets = false;
		return true;
	}

	// So, we found a value in between or higher than target2, lets first check if it was higher (or equal) than target2

	if (*lowerTarget2 >= target2)
	{
		foundInBetweenTargets = false;
		return true;
	}

	// Now we at least have 1 or more values in between targets, so lets add them all to the output
	for (auto it = lowerTarget2; it != lowerTarget1; it++)
	{
		foundInBetweenTargets = true;
		const int index = int(it - values.begin());
		// We already pushes this value from when we pushed around the first target
		if (foundAroundTarget && index == outputIndices[1]) return true;

		outputIndices.push_back(index);
	}

	return true;
}

bool skewTMaker::getAllIndicesAround(std::vector<float> values, bool& valueIsTarget1, bool& foundAroundTarget, bool& foundInBetweenTargets, std::vector<int>& outputIndices, float target1, float target2)
{
	valueIsTarget1 = false;
	foundAroundTarget = false;
	foundInBetweenTargets = false;

	if (values.empty() || target2 < target1) return false;

	// First get iterator to value lower than target2
	auto lower = std::upper_bound(values.begin(), values.end(), target2, std::greater<float>());

	// Lower is same as target  (also means that this was the lowest value in the vector)
	if (lower != values.end() && *lower == target2)
	{
		// First point is exactly the same as target, we do not push this one
		lower++;
	}

	// Loop over all in between targets
	while (lower != values.end() && *lower > target1)
	{
		foundInBetweenTargets = true;
		outputIndices.push_back(int(lower - values.begin()));

		if (*lower == target1)
		{
			valueIsTarget1 = true;
		}
		lower++;
	}
	if (lower != values.end())
	{
		foundAroundTarget = true;
		outputIndices.push_back(int(lower - values.begin()));
	}

	return !outputIndices.empty();
}

void skewTMaker::cameraControl(float dt)
{
	if (bee::Engine.Inspector().IsSelected())
	{
		return;
	}

	bool LeftShift = bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftShift);

	//For each camera (we have 1)
	for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
	{
		glm::vec3 newCamPos = transform.GetTranslation();

		//------------------------------------------------------------------------------
		//--------------------------Moving around---------------------------------------
		//------------------------------------------------------------------------------

		float cameraSpeed = 25.0f * dt * (m_zoomValue * 2.0f + 0.5f);
		if (LeftShift)
		{
			cameraSpeed *= 1.5f;
		}


		glm::vec3 forward = transform.GetRotation() * glm::vec3(0, 1, 0) * cameraSpeed;
		glm::vec3 right = transform.GetRotation() * glm::vec3(1, 0, 0) * cameraSpeed;


		//Using keys
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::W))
		{
			newCamPos = newCamPos + forward * cameraSpeed;
		}
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::A))
		{
			newCamPos = newCamPos - right * cameraSpeed;
		}
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::S))
		{
			newCamPos = newCamPos - forward * cameraSpeed;
		}
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::D))
		{
			newCamPos = newCamPos + right * cameraSpeed;
		}


		//------------------------------------------------------------------------------
		//--------------------------Zooming in/out--------------------------------------
		//------------------------------------------------------------------------------

		if (m_mouseWheel != bee::Engine.Input().GetMouseWheel()) //Mouse has been scrolled
		{
			float difference = bee::Engine.Input().GetMouseWheel() - m_mouseWheel;
			glm::vec3 dir = glm::normalize(m_mousePos3D - newCamPos); // Normalize to get just the direction
			if (difference < 0.0f) 
			{
				// With zooming out, we lerp between zooming from mousewheel to zooming to our (0) camera pos
				glm::vec3 newDir = glm::normalize(newCamPos - (m_zeroCamPos * glm::vec3(1, 1, -1) + glm::vec3(0, 1, 0))); // y + 1 to avoid normalizing 0
				dir = bee::Lerp(dir, newDir, m_zoomValue);
				if (newCamPos.y == m_minZoom) dir = glm::vec3(0); // No zoom when already at max distance
			}
			dir *= m_zoomValue * (m_minZoom - 0) * 0.2f;// set speed based on zoom value, distance between max and minus times only a part of it.
			//Casual P = O + D*T
			glm::vec3 newPos = newCamPos + dir * difference;
			if (newPos.y < m_maxZoom) newPos = glm::vec3(newCamPos.x, m_maxZoom, newCamPos.z);
			if (newPos.y > m_minZoom) newPos = glm::vec3(m_zeroCamPos.x, m_zeroCamPos.y, -m_zeroCamPos.z);

			newCamPos = newPos;
			m_zoomValue = (newPos.y - m_maxZoom) / (m_minZoom - m_maxZoom);
		}

		//------------------------------------------------------------------------------
		//----------------------------------Restrict------------------------------------
		//------------------------------------------------------------------------------

		{
			glm::vec2 minPos = m_usingHodograph ? glm::vec2(m_centerhodoPos.x, -m_centerhodoPos.z) - m_windMax * m_hodoGraphDecrease : m_skewTPos;
			glm::vec2 maxPos = m_usingHodograph ? glm::vec2(m_centerhodoPos.x, -m_centerhodoPos.z) + m_windMax * m_hodoGraphDecrease : m_skewTPos + m_skewTSize;
			glm::vec2 margin = m_usingHodograph ? glm::vec2(m_windMax * m_hodoGraphDecrease * (m_zoomValue + 0.1f)) : m_skewTSize * (m_zoomValue + 0.1f);
			margin.y *= 0.5f;


			// First check left and bottom
			glm::vec3 dir = bee::getCameraBottomLeftDir();
			float t = -newCamPos.y / dir.y;  // Direct intersection calculation for y=0 plane
			glm::vec3 pos = newCamPos + dir * t;
			// Set maximum value
			pos.x = std::max(pos.x, minPos.x - margin.x);
			pos.z = std::max(pos.z, minPos.y - margin.y);
			// Shoot back and set our new camera value
			newCamPos = pos + -dir * t;

			// Now Top and right
			dir = bee::getCameraTopRightDir();
			pos = newCamPos + dir * t;
			// Set minimum value
			pos.x = std::min(pos.x, maxPos.x + margin.x);
			pos.z = std::min(pos.z, maxPos.y + margin.y);
			// Shoot back and set our new camera value
			newCamPos = pos + -dir * t;
		}

		// Finally set our final value
		transform.SetTranslation(newCamPos);
	}
	

}

void skewTMaker::confirmSkewT()
{
	std::vector<float> altitude;
	std::vector<float> temps;
	std::vector<float> dews;
	std::vector<float> windSpeed;
	std::vector<glm::vec2> windDir;
	std::vector<float> windDirFloat;
	std::vector<float> pressure;


	// Add all pressures from temp, dew and wind
	for (int i = 0; i < int(m_simpleTemps.pressure.size()); i++) pressure.push_back(m_simpleTemps.pressure[i]);
	for (int i = 0; i < int(m_simpleDews.pressure.size()); i++) pressure.push_back(m_simpleDews.pressure[i]);
	for (int i = 0; i < int(m_simpleWindSpeed.pressure.size()); i++) pressure.push_back(m_simpleWindSpeed.pressure[i]);


	std::sort(pressure.begin(), pressure.end(), std::greater<float>()); // sort
	// Move all doubles to end of vector and get iterator to new end
	auto itToRemove = std::unique(pressure.begin(), pressure.end());
	// Remove all duplicates
	pressure.erase(itToRemove, pressure.end());

	const int MAXSIZE = 9999;
	if (pressure.size() > MAXSIZE)
	{
		printf("Error: size of data created is too large: %i/%i inputs registered, aborting...\n", int(pressure.size()), MAXSIZE);
		return;
	}

	// Add values at the beginning and end
	{
		pressure.push_back(1.0f);

		m_simpleTemps.value.push_back(-30.0f);
		m_simpleTemps.pressure.push_back(1.0f);

		m_simpleDews.value.push_back(-40.0f);
		m_simpleDews.pressure.push_back(1.0f);

		if (!m_simpleWindSpeed.pressure.empty()) m_simpleWindSpeed.value.push_back(m_simpleWindSpeed.value.back());
		else m_simpleWindSpeed.value.push_back(0.0f), m_simpleWindSpeed.value.push_back(0.0f), m_simpleWindSpeed.pressure.push_back(1000.0f); // If empty, add begin and end
		m_simpleWindSpeed.pressure.push_back(1.0f);

		if (!m_windDir.empty()) m_windDir.push_back(m_windDir.back());
		else m_windDir.push_back(glm::vec2(0, 0)), m_windDir.push_back(glm::vec2(0, 0)); // If empty, add begin and end

		temps.push_back(m_simpleTemps.value[0]);
		dews.push_back(m_simpleDews.value[0]);
		if (!m_simpleWindSpeed.pressure.empty()) windSpeed.push_back(m_simpleWindSpeed.value[0]);
		else windSpeed.push_back(0.0f);
		if (!m_windDir.empty()) windDir.push_back(m_windDir[0]);
		else windDir.push_back(glm::vec2(1, 0));
	}

	glm::vec2 windDirDummy{};
	int idxAtTemp = 1;
	int idxAtDew = 1;
	int idxAtWind = 1;

	// For each pressure at lerped value
	for (int i = 1; i < int(pressure.size()); i++)
	{

		// Temperature
		if (m_simpleTemps.pressure[idxAtTemp] == pressure[i])
		{
			temps.push_back(m_simpleTemps.value[idxAtTemp]);
			if (m_simpleTemps.value.size() - 1 > idxAtTemp) idxAtTemp++;
		}
		else
		{
			temps.push_back(lerpValues(m_simpleTemps, pressure, i, idxAtTemp, false, windDirDummy));
		}

		// Dew Point
		if (m_simpleDews.pressure[idxAtDew] == pressure[i])
		{
			dews.push_back(m_simpleDews.value[idxAtDew]);
			if (m_simpleDews.value.size() - 1 > idxAtDew) idxAtDew++;
		}
		else
		{
			dews.push_back(lerpValues(m_simpleDews, pressure, i, idxAtDew, false, windDirDummy));
		}

		// Wind Speed and direction
		if (m_simpleWindSpeed.pressure[idxAtWind] == pressure[i])
		{
			windSpeed.push_back(m_simpleWindSpeed.value[idxAtWind]);
			windDir.push_back(m_windDir[idxAtWind]);
			idxAtWind++;
		}
		else
		{
			windSpeed.push_back(lerpValues(m_simpleWindSpeed, pressure, i, idxAtWind, false, windDirDummy));
			lerpValues(m_simpleWindSpeed, pressure, i, idxAtWind, true, windDirDummy);
			windDir.push_back(windDirDummy);
		}
	}


	for (int i = 0; i < int(pressure.size()); i++)
	{
		altitude.push_back(meteoformulas::getStandardHeightAtPressure(temps[0], pressure[i]));
		// Change windDir to single float
		windDirFloat.push_back(atan2(windDir[i].x, windDir[i].y) * (180/Constants::PI) + 180.0f);
		windSpeed[i] *= 0.51444f; // Knots to m/s
	}

	//Copy the data over
	{
		Game.ReadTable().skewTData.data.allocate(pressure.size());
		std::memcpy(Game.ReadTable().skewTData.data.temperature, temps.data(), temps.size() * sizeof(float));
		std::memcpy(Game.ReadTable().skewTData.data.dewPoint, dews.data(), dews.size() * sizeof(float));

		std::memcpy(Game.ReadTable().skewTData.data.windDir, windDirFloat.data(), windDirFloat.size() * sizeof(float));

		std::memcpy(Game.ReadTable().skewTData.data.windSpeed, windSpeed.data(), windSpeed.size() * sizeof(float));

		std::memcpy(Game.ReadTable().skewTData.data.pressure, pressure.data(), pressure.size() * sizeof(float));
		std::memcpy(Game.ReadTable().skewTData.data.altitude, altitude.data(), altitude.size() * sizeof(float));
	}

	Game.ReadTable().initEnvironment();

	m_skewTConfirmed = true;
	doneMakingSkewT = true;
}

void skewTMaker::resetControls()
{
	m_mouseWheel = bee::Engine.Input().GetMouseWheel(); //Update scroll 
}


void skewTMaker::shortCuts()
{

	if (bee::Engine.Input().GetKeyboardKeyOnce(bee::Input::KeyboardKey::Tab))
	{
		if (m_currentParam == TEMPERATURE) m_currentParam = DEWPOINT, m_cursorActive = false, m_editingPoint = -1, updateCursor(true);
		else if (m_currentParam == DEWPOINT) m_currentParam = TEMPERATURE, m_cursorActive = false, m_editingPoint = -1, updateCursor(true);
	}

}

//-------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------               Drawing same as skewTer                    --------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------
void skewTMaker::drawBackground()
{

	// Skew-T

	for (float p = 100; p < 1000; p += 100)
	{
		glm::vec2 coords = convertToPlottingCoordinates(0, p, true);

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(m_skewTPos.x, 0, -coords.y), glm::vec3(m_skewTPos.x + m_skewTSize.x, 0, -coords.y), bee::Colors::Grey);
	}
	for (float i = m_tempMin - 50; i <= m_tempMax; i += 1)
	{
		glm::vec2 coords = convertToPlottingCoordinates(i, 100, true);
		glm::vec2 coords2 = convertToPlottingCoordinates(i, 1000, true);

		glm::vec4 color = color = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);;
		if (int(i) % 5 == 0) color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
		if (int(i) % 10 == 0) color = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
		if (i == 0) color = bee::Colors::White;

		if (coords2.x < m_skewTPos.x)
		{
			float a = (coords.x - m_skewTPos.x) / (coords.x - coords2.x);
			float y3 = a * (coords2.y - coords.y) - coords2.y;
			coords2 = glm::vec2(m_skewTPos.x, y3);
		}
		if (coords.x > m_skewTPos.x + m_skewTSize.x)
		{
			float a = (coords.x - (m_skewTPos.x + m_skewTSize.x)) / (coords.x - coords2.x);
			float y3 = a * (coords2.y - coords.y) - coords2.y;
			coords = glm::vec2(m_skewTPos.x + m_skewTSize.x, y3);
		}


		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords2.x, 0, -coords2.y), glm::vec3(coords.x, 0, -coords.y), color);
	}

	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(m_skewTPos.x, 0, -m_skewTPos.y), glm::vec3(m_skewTPos.x + m_skewTSize.x, 0, -m_skewTPos.y), bee::Colors::Black);
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(m_skewTPos.x, 0, -m_skewTPos.y), glm::vec3(m_skewTPos.x, 0, -(m_skewTPos.y + m_skewTSize.y)), bee::Colors::Black);


	// Hodograph
	{
		for (float wind = 10.0f * m_hodoGraphDecrease; wind <= m_windMax * m_hodoGraphDecrease; wind += 10.0f * m_hodoGraphDecrease)
		{
			// Windspeed in knots
			bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, glm::vec3(m_centerhodoPos.x, 0.0f, -m_centerhodoPos.z), wind, glm::vec3(0, 1, 0), bee::Colors::Grey);
		}

		// Axis
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(m_centerhodoPos.x - m_windMax * m_hodoGraphDecrease, 0.0f, -m_centerhodoPos.z), glm::vec3(m_centerhodoPos.x + m_windMax * m_hodoGraphDecrease, 0.0f, -m_centerhodoPos.z), bee::Colors::Black);
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(m_centerhodoPos.x, 0.0f, -(m_centerhodoPos.z - m_windMax * m_hodoGraphDecrease)), glm::vec3(m_centerhodoPos.x, 0.0f, -(m_centerhodoPos.z + m_windMax * m_hodoGraphDecrease)), bee::Colors::Black);
	}
}

void skewTMaker::drawEnvironment()
{
	//Temp and dewpoint
	{
		for (int i = 1; i < int(m_simpleTemps.value.size()); i++)
		{
			glm::vec2 tempCoords = convertToPlottingCoordinates(m_simpleTemps.value[i], m_simpleTemps.pressure[i], true);
			glm::vec2 tempPrevCoords = convertToPlottingCoordinates(m_simpleTemps.value[i - 1], m_simpleTemps.pressure[i - 1], true);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(tempCoords.x, 0.0f, -tempCoords.y), glm::vec3(tempPrevCoords.x, 0.0f, -tempPrevCoords.y), bee::Colors::Red);
		}
		for (int i = 1; i < int(m_simpleDews.value.size()); i++)
		{
			glm::vec2 dewCoords = convertToPlottingCoordinates(m_simpleDews.value[i], m_simpleDews.pressure[i], true);
			glm::vec2 dewPrevCoords = convertToPlottingCoordinates(m_simpleDews.value[i - 1], m_simpleDews.pressure[i - 1], true);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dewCoords.x, 0.0f, -dewCoords.y), glm::vec3(dewPrevCoords.x, 0.0f, -dewPrevCoords.y), bee::Colors::Green);
		}
		for (int i = 0; i < int(m_simpleWindSpeed.value.size()); i++)
		{
			if (i > 0)
			{
				glm::vec3 windCoords = convertWindToCoordinates(m_simpleWindSpeed.value[i], m_windDir[i]);
				glm::vec3 windPrevCoords = convertWindToCoordinates(m_simpleWindSpeed.value[i - 1], m_windDir[i - 1]);

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(windCoords.x, 0.0f, windCoords.z), glm::vec3(windPrevCoords.x, 0.0f, windPrevCoords.z), getColorWindHeight(meteoformulas::getStandardHeightAtPressure(0, m_simpleWindSpeed.pressure[i - 1])));
			}
			drawWindFlag(m_simpleWindSpeed.pressure[i], m_simpleWindSpeed.value[i], m_windDir[i]);
		}
	}
}

void skewTMaker::drawDryAndMoist()
{
	//Dry and moist adiabatics
	{
		if (m_simpleTemps.value.size() == 0 || m_simpleDews.value.size() == 0 || m_simpleTemps.value[0] != m_simpleTemps.value[0] || m_simpleTemps.value[0] <= -273.0f ||
			m_simpleDews.value[0] != m_simpleDews.value[0]) return;

		// Create canvas pressure vector
		static std::vector<float> pressureCanvas{};
		float tempPres = m_simpleTemps.pressure[0];
		float decreaseWith = 0.0f;
		if (pressureCanvas.empty())
		{
			do
			{
				pressureCanvas.push_back(tempPres);
				decreaseWith = 2000.0f / tempPres; // Make sure it decrease more with lower pressure
				tempPres -= decreaseWith;
			} while (tempPres > 100.0f);
		}

		std::unique_ptr<float[]> temps = std::make_unique<float[]>(pressureCanvas.size());

		//Dry adiabatic to LCL
		meteoformulas::getDryAdiabatic(m_simpleTemps.value[0], pressureCanvas[0], pressureCanvas.data(), temps.get(), pressureCanvas.size());

		for (int j = 1; j < int(pressureCanvas.size()); j++)
		{
			if (temps[j - 1] <= -273.0f || temps[j] <= -273.0f) return;
			//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
			glm::vec2 coords = convertToPlottingCoordinates(temps[j], pressureCanvas[j], true);
			glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], pressureCanvas[j - 1], true);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, 0, -coords.y), glm::vec3(coordsPrev.x, 0, -coordsPrev.y), bee::Colors::Black);
		}

		//Moist adiabatic at LCL
		int offset = 0;
		glm::vec3 LCL = meteoformulas::getLCL(m_simpleTemps.value[0], pressureCanvas[0], 0, m_simpleDews.value[0]);
		meteoformulas::getMoistTemp(LCL.x, LCL.y, pressureCanvas.data(), temps.get(), pressureCanvas.size(), offset);
		if (offset != -1)
		{
			for (int j = offset + 1; j < int(pressureCanvas.size()); j++)
			{
				if (temps[j - 1] <= -273.0f || temps[j] <= -273.0f) return;
				//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
				glm::vec2 coords = convertToPlottingCoordinates(temps[j], pressureCanvas[j], true);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], pressureCanvas[j - 1], true);

				bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, 0, -coords.y), glm::vec3(coordsPrev.x, 0, -coordsPrev.y), bee::Colors::Black);
			}
		}
	}
}

void skewTMaker::tooltipsHelp()
{
	const float amountOfTips = 32;

	// Based on max and min value on screen we decide which values we want to show

	glm::vec2 maxValues = glm::vec2(0);
	glm::vec2 minValues = glm::vec2(0);
	glm::vec3 topRight = bee::screenToGround(glm::vec2(bee::Engine.Device().GetWidth(), bee::Engine.Device().GetHeight()));
	glm::vec3 bottomLeft = bee::screenToGround(glm::vec2(0));
	convertToTempAndPressure(glm::vec2(topRight.x, topRight.z), maxValues);
	convertToTempAndPressure(glm::vec2(bottomLeft.x, bottomLeft.z), minValues);

	glm::vec2 differences = maxValues - minValues;
	glm::vec2 roundTo = glm::vec2(10, 100);

	// Start with temp
	if (differences.x > 25) roundTo.x = 10;
	else roundTo.x = 1;
	// Now pressure
	if (differences.y > 250) roundTo.y = 100;
	else if (differences.y > 25) roundTo.y = 10;
	else roundTo.y = 1;


	// For x amount of times, get values and round them to the designated values
	glm::vec3 checkPos = glm::vec3(0);
	glm::vec2 previousValue = glm::vec2(-999);
	for (float i = 0; i <= amountOfTips; i++)
	{
		// Get part of the way to max and multiply by size of screen to get coordinates at this tooltip
		checkPos = bee::screenToGround(glm::vec2(float(bee::Engine.Device().GetWidth()) * (i / amountOfTips), float(bee::Engine.Device().GetHeight()) * (i / amountOfTips)));
		// Get the value
		glm::vec2 valuesAt = glm::vec2(0); 
		convertToTempAndPressure(glm::vec2(checkPos.x, checkPos.z), valuesAt);
		// Make values go below decimal, round this up or down and put values back
		valuesAt = glm::round(valuesAt / roundTo) * roundTo;


		glm::vec2 finalPosTemp = convertToPlottingCoordinates(valuesAt.x, maxValues.y, true);
		glm::vec2 finalPosPres = convertToPlottingCoordinates(m_tempMin, valuesAt.y, true);
		finalPosTemp = bee::PosToScreen(glm::vec3(finalPosTemp.x, 0.0f, -finalPosTemp.y));
		finalPosPres = bee::PosToScreen(glm::vec3(finalPosPres.x, 0.0f, -finalPosPres.y));

		// Finally show Tooltip
		if (previousValue.x != valuesAt.x)
		{
			std::string label = "ToolTipTemp" + std::to_string(i);
			ImGui::Begin(label.c_str(), 0, ImGuiWindowFlags_NoMove  | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

			ImGui::SetWindowPos(ImVec2(finalPosTemp.x, bee::Engine.Device().GetHeight() - 100.0f));
			ImGui::Text("%i", int(valuesAt.x));
			ImGui::End();
		}
		if (previousValue.y != valuesAt.y)
		{
			std::string label = "ToolTipPres" + std::to_string(i);
			ImGui::Begin(label.c_str(), 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
			ImGui::SetWindowPos(ImVec2(100, finalPosPres.y));
			ImGui::Text("%i", int(valuesAt.y));
			ImGui::End();
		}

		previousValue = valuesAt;
	}
}


glm::vec2 skewTMaker::convertToPlottingCoordinates(const float temp, const float value, const bool pressure)
{
	//Respect hPa for height in meter using standard pressure
	float height = value;
	if (!pressure) height = meteoformulas::getStandardPressureAtHeight(0, value); //Could use pressure from data.


	//---------------------Log()----------------------

	height = (log10f(height) - log10f(100)) / (log10f(1000) - log10f(100)) * m_skewTSize.y;
	height = m_skewTSize.y - height;

	//-------------------------------------------------


	//Skew value
	float skewedTemp = temp + m_skewed * (height / m_skewTSize.y) * (m_tempMax - m_tempMin);


	// Offset and return
	return { skewedTemp + m_skewTPos.x - m_tempMin, height + m_skewTPos.y };
}


bool skewTMaker::convertToTempAndPressure(glm::vec2 position, glm::vec2& output)
{
	bool succeeded = true;
	position.y = -position.y;

	if (position.x < m_skewTPos.x || position.x > m_skewTPos.x + m_skewTSize.x ||
		position.y < m_skewTPos.y || position.y > m_skewTPos.y + m_skewTSize.y)
	{
		// Invalid but try to clamp at least
		position.x = std::clamp(position.x, m_skewTPos.x, m_skewTPos.x + m_skewTSize.x);
		position.y = std::clamp(position.y, m_skewTPos.y, m_skewTPos.y + m_skewTSize.y);
		succeeded = false;
	}

	// Adjust for offset
	position -= m_skewTPos;
	// Adjust x
	position.x += m_tempMin;
	

	// Un-skew the x coordinate at pressure
	// 
	// First we get the skewing
	float temp = m_skewed * (position.y / m_skewTSize.y) * (m_tempMax - m_tempMin);
	// Then we remove that
	temp = position.x - temp;


		// Get pressure from y using the reverse of how we got y from pressure.
	// 
	// First undo the flipping
	float pressure = m_skewTSize.y - position.y;
	// Then the scaling from 0-height to 0-1
	pressure /= m_skewTSize.y;
	// Undo log normalization (The '/ (log10f(1000) - log10f(100)' part) 
	pressure *= log10f(1000) - log10f(100);
	// Undo mapping to 0
	pressure += log10f(100);
	// Lastly, undo the logging
	pressure = powf(10.0f, pressure);


	output = glm::vec2(temp, pressure);
	return succeeded;
}

void skewTMaker::convertCoordinatesToWind(glm::vec3 position, float& windSpeedOutput, glm::vec2& windDirOutput)
{
	// Simply just get the distance to the center pos, then divide by decrease in size
	glm::vec3 realCenter = glm::vec3(m_centerhodoPos.x, m_centerhodoPos.y, -m_centerhodoPos.z);
	windSpeedOutput = distance(position, realCenter);
	windSpeedOutput /= m_hodoGraphDecrease; // Divide due to it being an inverse

	glm::vec3 dir = position - realCenter;
	windDirOutput = glm::normalize(glm::vec2(dir.x, dir.z));
}

glm::vec3 skewTMaker::convertWindToCoordinates(float windSpeed, glm::vec2 windDirection)
{
	windSpeed *= m_hodoGraphDecrease; // Undo mapping to decrease, we now have basically distance
	glm::vec3 realCenter = glm::vec3(m_centerhodoPos.x, m_centerhodoPos.y, -m_centerhodoPos.z);
	return realCenter + glm::vec3(windDirection.x, 0.0f, windDirection.y) * windSpeed;
}

void skewTMaker::drawWindFlag(float pressureHeight, float windSpeed, glm::vec2 windDir)
{
	const float windBarOffset = -5.0f;
	const float windBarSize = 7.5f; // Size of the bar
	const float windStickSize = 3.0f; // Size of the sticks

	// Get correct height by using height to pressure and getting coordinates
	float height = meteoformulas::getStandardHeightAtPressure(0, pressureHeight);
	glm::vec2 tempPos = convertToPlottingCoordinates(0.0f, pressureHeight, true);

	// Now get the correct position
	glm::vec3 position = glm::vec3(m_skewTPos.x + m_skewTSize.x + windBarOffset, 0.0f, -tempPos.y);
	glm::vec3 endPos = position + glm::vec3(windDir.x * windBarSize, 0.0f, windDir.y * windBarSize);

	glm::vec4 color = getColorWindHeight(height);

	// Already draw main bar
	bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::General, position, endPos, color);

	// Now time for the 'flag' part
	glm::vec3 dir = glm::vec3(windDir.x, 0, windDir.y);
	glm::vec3 perpDir = glm::vec3(windDir.y, 0.0f, -windDir.x);

	// Loop over all different shapes for wind speeds (50 knots, 10 knots and 1 for 5 knots)
	const float offsetSingle = windBarSize * 0.05f;
	float tempWindSpeed = windSpeed;
	// Create flag
	while (tempWindSpeed >= 50.0f) 
	{
		tempWindSpeed -= 50.0f;
		glm::vec3 endStickPos = endPos + perpDir * windStickSize;

		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::General, endPos, endStickPos, color);
		// For 50 knots flags we grab end of 1 stick and drag that to new stick pos, creating a long triangle
		endPos -= dir * offsetSingle;
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::General, endPos, endStickPos, color);

		endPos -= dir * offsetSingle; // Offset for next
	}
	// Long stick
	while (tempWindSpeed >= 10.0f) 
	{
		tempWindSpeed -= 10.0f;
		glm::vec3 endStickPos = endPos + perpDir * windStickSize;
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::General, endPos, endStickPos, color);
		endPos -= dir * offsetSingle; // Offset for next
	}
	// Short stick
	if (tempWindSpeed >= 5.0f) 
	{
		glm::vec3 endStickPos = endPos + perpDir * windStickSize * 0.5f; // Half the size
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::General, endPos, endStickPos, color);
	}
}

glm::vec4 skewTMaker::getColorWindHeight(float height)
{
	if (height < 1000.0f) return glm::vec4(0.7f, 0, 0, 1.0f); // Red
	else if (height < 3000.0f) return glm::vec4(0.9f, 0.45f, 0, 1.0f); // Orange
	else if (height < 6000.0f) return glm::vec4(0.9f, 0.9f, 0, 1.0f); // Yellow
	else if (height < 10000.0f) return glm::vec4(0.2f, 0.9f, 0.2f, 1.0f); // Green
	else return glm::vec4(0.7f, 0.7f, 1.0f, 1.0f); // light blue
}

float skewTMaker::lerpValues(const simpleData& data, const std::vector<float>& pressures, const int i, const int idxAt, const bool windDir, glm::vec2& windDirection)
{
	// Lerp to get temp in between, use log to get correct coordinates
	if (idxAt >= data.pressure.size())
	{
		if (windDir) windDirection = m_windDir.empty() ? glm::vec2(0) : m_windDir.back();
		return data.pressure.empty() ? 0.0f : data.pressure.back();
	}
	float logPCur = std::log(data.pressure[idxAt]); 
	float logPPrev = std::log(data.pressure[idxAt - 1]);
	float logPTarget = std::log(pressures[i]);
	float r = (logPCur - logPTarget) / (logPCur - logPPrev);

	if (windDir) windDirection = glm::mix(m_windDir[idxAt], m_windDir[idxAt - 1], r);
	return bee::Lerp(data.value[idxAt], data.value[idxAt - 1], r);
}
