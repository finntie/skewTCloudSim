#pragma once
#include "config.h"
#include <vector>
#include <glm/glm.hpp>

class skewTMaker
{
public:

	void init();
	void update(float dt);

	void panel();

	bool doneMakingSkewT{ false };

private:
	void drawBackground();
	void drawEnvironment();
	void drawDryAndMoist();
	void tooltipsHelp();

	void handleMouseInput();
	void updateCursor(bool force);
	void cameraControl(float dt);
	void confirmSkewT();
	void resetControls();

	//Converts temp and height in meters or pressure to plotting so its easily modified.
	//Warning: Not using pressure (so height in meters) could lead up to different values due to observed not being the same as standard.
	glm::vec2 convertToPlottingCoordinates(const float temp, const float value, const bool pressure);

	bool convertToTempAndPressure(glm::vec2 position, glm::vec2& output);

	void convertCoordinatesToWind(glm::vec3 position, float& windSpeedOutput, glm::vec2& windDirOutput);
	glm::vec3 convertWindToCoordinates(float windSpeed, glm::vec2 windDirection);

	glm::vec2 m_skewTSize{ 100, 100 };
	glm::vec2 m_skewTPos{ -50, -50 };
	glm::vec2 m_hodographOffset{ 0, 0 }; // Offset on the right of the skewT and a down from the top 
	glm::vec3 m_centerSkewTPos{ 0,0,0 };
	glm::vec3 m_centerhodoPos{ 0,0,0 };
	float m_tempMin = -40; // Minimum temp on the skew-T
	float m_tempMax = 40; // Maximum temp on the skew-T
	float m_windMax = 60.0f; // Max wind speed on hodograph in knots
	float m_hodoGraphDecrease = 1.0f / 2.0f; // Making the hodograph smaller to fit more numbers.

	std::vector<float> m_temps;
	std::vector<float> m_dews;
	std::vector<float> m_pressure;
	std::vector<float> m_windSpeed;
	std::vector<glm::vec2> m_windDir;
	glm::vec2 m_minPos { 0, 0 };
	glm::vec2 m_maxPos { 0, 0 };
	glm::vec3 m_mousePos3D{ 0, 0, 0 };

	glm::vec3 m_cursorPos{ 0,0,0 }; // Pos at previous location (or current if no previous was set)
	glm::vec3 m_targetPos{ 0,0,0 }; // Pos at possible next position

	float m_mouseWheel = 0;
	float m_zoomValue = 1.0f;
	float m_minZoom = 100.0f;
	float m_maxZoom = 1.0f;
	glm::vec3 m_zeroCamPos{ 0, 100, 0 }; // Start pos of camera

	bool m_tempsDone{ false };
	bool m_skewTReady{ false };
	bool m_skewTConfirmed{ false };
	bool m_usingHodograph{ false };

	// QOL values
	bool m_cursorActive{ false };
	float m_skewed = glm::tan(glm::radians(45.0f));
};

