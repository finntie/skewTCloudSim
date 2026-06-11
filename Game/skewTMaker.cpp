#include "pch.h"
#include "skewTMaker.h"

#include "math/meteoformulas.h"
#include "math/math.hpp"
#include "readTable.h"
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

void skewTMaker::update(float dt)
{
	m_mousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());


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
	const char* hodographLabel = "Go to Hodograph";
	if (m_usingHodograph) hodographLabel = "Return to Skew-T";
	if (ImGui::Button(hodographLabel))
	{
		if (m_usingHodograph)
		{
			m_minZoom = 100.0f;
			m_zeroCamPos = m_centerSkewTPos + glm::vec3(0, m_minZoom, 0);
		}
		else
		{
			m_minZoom = 50.0f;
			m_zeroCamPos = m_centerhodoPos + glm::vec3(0, m_minZoom, 0);
		}
		for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
		{
			transform.SetTranslation(glm::vec3(m_zeroCamPos.x, m_zeroCamPos.y, -
				m_zeroCamPos.z));
		}
		m_usingHodograph = !m_usingHodograph;
	}
	tooltipsHelp();
}




void skewTMaker::handleMouseInput()
{
	if (m_skewTConfirmed || m_skewTReady) return;

	// Set cursor and target pos with respect to boundaries
	{
		updateCursor(false);
	}

	glm::vec2 tempAt = glm::vec2(0);
	
	if (m_cursorActive && convertToTempAndPressure(glm::vec2(m_targetPos.x, m_targetPos.z), tempAt))
	{
		bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, m_cursorPos, m_targetPos, glm::vec4(0, 0.8f, 0, 1));

		if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left)) // Confirming
		{
			if (!m_tempsDone)
			{
				m_temps.push_back(tempAt.x);
				m_pressure.push_back(tempAt.y);
			}
			else m_dews.push_back(tempAt.x);

			if (m_targetPos.z == -(m_skewTPos.y + m_skewTSize.y))
			{
				m_tempsDone = true;
				m_cursorActive = false;
				glm::vec2 zeroCoord = convertToPlottingCoordinates(m_temps[0], m_pressure[0], true);
				m_cursorPos = glm::vec3(zeroCoord.x, 0, -zeroCoord.y);
			}
			if (m_dews.size() == m_temps.size()) m_skewTReady = true;
		}
	}

	// (Previous) selecting cursor
	{
		// If cursor is near
		float size = 0.5f;
		glm::vec4 color = glm::vec4(1.0f);
		if (!m_cursorActive && distance(m_mousePos3D, m_cursorPos) <= 1.5f)
		{
			size = 0.6f;
			color = glm::vec4(0, 0.6f, 0, 1); // Dark green
			if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
			{
				m_cursorActive = true;
				size = 0.4f;
				color = glm::vec4(0, 0.9f, 0, 1); // Light green
			}
		}
		else if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Right))
		{
			if (m_cursorActive) m_cursorActive = false;
			else
			{
				// Undo previous
				if (m_tempsDone && !m_dews.empty())
				{
					m_dews.pop_back();
					updateCursor(true);
				}
				else if (!m_tempsDone && !m_temps.empty())
				{
					m_temps.pop_back();
					m_pressure.pop_back();
					updateCursor(true);
				}
			}
		}
		

		bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::General, m_cursorPos, size * (m_zoomValue + 0.2f), glm::vec3(0, 1, 0), color);
		if (m_cursorActive) bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::General, m_cursorPos, size * (m_zoomValue + 0.2f), glm::vec3(0, 1, 0), color);
	}
}

void skewTMaker::updateCursor(bool force)
{
	const float mininumHeightAdd = 0.25f; // In coordinates (not real height/pressure)
	const float maxHeightAdd = 25.0f;
	
	if (m_cursorActive || force)
	{
		if ((!m_tempsDone && m_temps.empty()) || (m_tempsDone && m_dews.empty()))
		{
			// Set cursor when at empty temp or dew
			m_targetPos.x = m_cursorPos.x = m_mousePos3D.x;
			m_cursorPos.z = -convertToPlottingCoordinates(0, 1000, true).y;
			m_targetPos.z = m_cursorPos.z;
		}
		else if (!m_tempsDone)
		{
			m_targetPos = m_mousePos3D;
			glm::vec2 plotCoords = convertToPlottingCoordinates(m_temps[int(m_temps.size()) - 1], m_pressure[int(m_pressure.size()) - 1], true);
			m_cursorPos.x = plotCoords.x;
			m_cursorPos.z = -plotCoords.y;
			m_targetPos.z = std::clamp(m_targetPos.z, m_cursorPos.z - maxHeightAdd, m_cursorPos.z - mininumHeightAdd); // Make sure we always go up by at least 1 (not downwards) and maximal upwards by 5
			m_targetPos.z = std::max(m_targetPos.z, -(m_skewTPos.y + m_skewTSize.y));
		}
		else
		{
			m_targetPos = m_mousePos3D;
			// Clamp to the same height as the temp was
			glm::vec2 plotCoords = convertToPlottingCoordinates(m_dews[int(m_dews.size()) - 1], m_pressure[int(m_dews.size()) - 1], true);
			m_cursorPos.x = plotCoords.x;
			m_cursorPos.z = -plotCoords.y;
		}
		if (m_tempsDone) // If only at dew empty or not
		{
			glm::vec2 plotCoords = convertToPlottingCoordinates(m_temps[int(m_dews.size())], m_pressure[int(m_dews.size())], true);
			m_targetPos.x = std::min(m_targetPos.x, plotCoords.x);
			m_targetPos.z = -plotCoords.y;
		}
	}
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

	// Add extra value for top
	{
		m_pressure.push_back(1.0f);
		m_temps.push_back(-30.0f);
		m_dews.push_back(-40.0f);
	}

	for (auto p : m_pressure)
	{
		altitude.push_back(meteoformulas::getStandardHeightAtPressure(m_temps[0], p));
	}

	//Copy the data over
	{
		Game.ReadTable().skewTData.data.allocate(m_temps.size());
		std::memcpy(Game.ReadTable().skewTData.data.temperature, m_temps.data(), m_temps.size() * sizeof(float));
		std::memcpy(Game.ReadTable().skewTData.data.dewPoint, m_dews.data(), m_temps.size() * sizeof(float));

		//std::memcpy(Game.ReadTable().skewTData.data.windDir, windDir.data(), m_temps.size() * sizeof(float));
		std::memset(Game.ReadTable().skewTData.data.windDir, 0, m_temps.size() * sizeof(float));

		//std::memcpy(Game.ReadTable().skewTData.data.windSpeed, windSpeed.data(), m_temps.size() * sizeof(float));
		std::memset(Game.ReadTable().skewTData.data.windSpeed, 0, m_temps.size() * sizeof(float));

		std::memcpy(Game.ReadTable().skewTData.data.pressure, m_pressure.data(), m_temps.size() * sizeof(float));
		std::memcpy(Game.ReadTable().skewTData.data.altitude, altitude.data(), m_temps.size() * sizeof(float));
	}

	Game.ReadTable().initEnvironment();

	m_skewTConfirmed = true;
	doneMakingSkewT = true;
}

void skewTMaker::resetControls()
{
	m_mouseWheel = bee::Engine.Input().GetMouseWheel(); //Update scroll 
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
		for (int i = 1; i < int(m_temps.size()); i++)
		{
			glm::vec2 tempCoords = convertToPlottingCoordinates(m_temps[i], m_pressure[i], true);
			glm::vec2 tempPrevCoords = convertToPlottingCoordinates(m_temps[i - 1], m_pressure[i - 1], true);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(tempCoords.x, 0.0f, -tempCoords.y), glm::vec3(tempPrevCoords.x, 0.0f, -tempPrevCoords.y), bee::Colors::Red);
		}
		for (int i = 1; i < int(m_dews.size()); i++)
		{
			glm::vec2 dewCoords = convertToPlottingCoordinates(m_dews[i], m_pressure[i], true);
			glm::vec2 dewPrevCoords = convertToPlottingCoordinates(m_dews[i - 1], m_pressure[i - 1], true);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(dewCoords.x, 0.0f, -dewCoords.y), glm::vec3(dewPrevCoords.x, 0.0f, -dewPrevCoords.y), bee::Colors::Green);
		}
	}
}

void skewTMaker::drawDryAndMoist()
{
	//Dry and moist adiabatics
	{
		if (m_temps.size() == 0 || m_dews.size() == 0 || m_temps[0] != m_temps[0] || m_temps[0] <= -273.0f ||
			m_dews[0] != m_dews[0]) return;

		std::unique_ptr<float[]> temps = std::make_unique<float[]>(m_temps.size());

		//Dry adiabatic to LCL
		meteoformulas::getDryAdiabatic(m_temps[0], m_pressure[0], m_pressure.data(), temps.get(), m_temps.size());

		for (int j = 1; j < int(m_temps.size()); j++)
		{
			if (temps[j - 1] <= -273.0f || temps[j] <= -273.0f) return;
			//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
			glm::vec2 coords = convertToPlottingCoordinates(temps[j], m_pressure[j], true);
			glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], m_pressure[j - 1], true);

			bee::Engine.DebugRenderer().AddLine(bee::DebugCategory::All, glm::vec3(coords.x, 0, -coords.y), glm::vec3(coordsPrev.x, 0, -coordsPrev.y), bee::Colors::Black);
		}

		//Moist adiabatic at LCL
		int offset = 0;
		glm::vec3 LCL = meteoformulas::getLCL(m_temps[0], m_pressure[0], 0, m_dews[0]);
		meteoformulas::getMoistTemp(LCL.x, LCL.y, m_pressure.data(), temps.get(), m_temps.size(), offset);
		if (offset != -1)
		{
			for (int j = offset + 1; j < int(m_temps.size()); j++)
			{
				if (temps[j - 1] <= -273.0f || temps[j] <= -273.0f) return;
				//TODO: should convertToPlottingCoordinates include setting default pressure height? (maybe an extra function that sets it)
				glm::vec2 coords = convertToPlottingCoordinates(temps[j], m_pressure[j], true);
				glm::vec2 coordsPrev = convertToPlottingCoordinates(temps[j - 1], m_pressure[j - 1], true);

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
	windSpeedOutput = distance(position, m_centerhodoPos);
	windSpeedOutput /= m_hodoGraphDecrease; // Divide due to it being an inverse

	glm::vec3 dir = position - m_centerhodoPos;
	windDirOutput = glm::normalize(glm::vec2(dir.x, dir.z));
}

glm::vec3 skewTMaker::convertWindToCoordinates(float windSpeed, glm::vec2 windDirection)
{
	windSpeed *= m_hodoGraphDecrease; // Undo mapping to decrease, we now have basically distance

	return m_centerhodoPos + glm::vec3(windDirection.x, 0.0f, windDirection.y) * windSpeed;
}
