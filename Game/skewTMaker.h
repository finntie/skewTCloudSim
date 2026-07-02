#pragma once
#include "config.h"
#include <vector>
#include <glm/glm.hpp>

class skewTMaker
{
public:

	void init();

	void loadData(std::vector<float>& temp, std::vector<float>& dew, std::vector<float>& pressure, std::vector<float>& windSpeed, std::vector<glm::vec2>& windDir);

	void update(float dt);

	void panel();

	bool doneMakingSkewT{ false };

private:

	struct simpleData
	{
		std::vector<float> value;
		std::vector<float> pressure;
	};

	enum skewTParams
	{
		TEMPERATURE, DEWPOINT, WIND
	};


	void shortCuts();

	void drawBackground();
	void drawEnvironment();
	void drawDryAndMoist();
	void tooltipsHelp();

	void handleMouseInput();
	void updateCursor(bool reset);
	void checkEdgeCasesTemp();
	void checkEdgeCasesDew();
	void fillInBetweens(std::vector<int>& posses, simpleData& usingData, simpleData& checkingData, glm::vec2 currentData, glm::vec2 targetData, bool& endedInOtherSector, glm::vec2& iPoint);

	void visualizeAndConfirmCursor();
	void visualizeAndConfirmCursorEditing();
	void selectUnselectCursor();
	void undo();
	bool lastValidChecks();
	bool getNearestIndex(std::vector<float> values, bool& foundNearest, int& closestPoint, float target);
	// Get point(s) at or around the target(s). Target 1 must be !lower! than target 2 (since we are working with pressure and lower value is higher up)
	bool getNearbyIndices(std::vector<float> values, bool& valueIsTarget, bool& foundAroundTarget, bool& foundInBetweenTargets, std::vector<int>& outputIndices, float target1, float target2 = -1);
	// Get all points in between target 1 and 2 and if not the same as target 1, then also one above target 1.
	bool getAllIndicesAround(std::vector<float> values, bool& valueIsTarget1, bool& foundAroundTarget, bool& foundInBetweenTargets, std::vector<int>& outputIndices, float target1, float target2);

	void cameraControl(float dt);
	void confirmSkewT();
	void resetControls();

	//Converts temp and height in meters or pressure to plotting so its easily modified.
	//Warning: Not using pressure (so height in meters) could lead up to different values due to observed not being the same as standard.
	glm::vec2 convertToPlottingCoordinates(const float temp, const float value, const bool pressure);

	bool convertToTempAndPressure(glm::vec2 position, glm::vec2& output);

	void convertCoordinatesToWind(glm::vec3 position, float& windSpeedOutput, glm::vec2& windDirOutput);
	glm::vec3 convertWindToCoordinates(float windSpeed, glm::vec2 windDirection);

	// Draws wind flag next to the skewT at height in meters, windspeed in knots
	void drawWindFlag(float pressure, float windSpeed, glm::vec2 windDir);

	// Height in Meters
	glm::vec4 getColorWindHeight(float height);

	float lerpValues(const simpleData& data, const std::vector<float>& pressures, const int i, const int idxAt, const bool windDir, glm::vec2& windDirection);

	glm::vec2 m_skewTSize{ 100, 100 };
	glm::vec2 m_skewTPos{ -50, -50 };
	glm::vec2 m_hodographOffset{ 0, 0 }; // Offset on the right of the skewT and a down from the top 
	glm::vec3 m_centerSkewTPos{ 0,0,0 };
	glm::vec3 m_centerhodoPos{ 0,0,0 };
	float m_tempMin = -40; // Minimum temp on the skew-T
	float m_tempMax = 40; // Maximum temp on the skew-T
	float m_windMax = 60.0f; // Max wind speed on hodograph in knots
	float m_hodoGraphDecrease = 1.0f / 2.0f; // Making the hodograph smaller to fit more numbers.
	const float m_mininumHeightAdd = 0.05f; // In coordinates (not real height/pressure)
	const float m_maxHeightAdd = 25.0f; // In coordinates (not real height/pressure)

	simpleData m_simpleTemps;
	simpleData m_simpleDews;
	simpleData m_simpleWindSpeed;
	std::vector<glm::vec2> m_windDir; // Connected with m_simpleWindSpeed
	glm::vec2 m_minPos { 0, 0 };
	glm::vec2 m_maxPos { 0, 0 };
	glm::vec3 m_mousePos3D{ 0, 0, 0 };

	glm::vec3 m_cursorPos{ 0,0,0 }; // Pos at previous location (or current if no previous was set)
	glm::vec3 m_targetPos{ 0,0,0 }; // Pos at possible next position
	std::vector<glm::vec3> m_inBetweenPosses; // Possibly posses in between to make sure not go below dew or above temp.
	float m_currentWindHeight = 1000.0f; // At current wind height in pressure

	float m_mouseWheel = 0;
	float m_zoomValue = 1.0f;
	float m_minZoom = 100.0f;
	float m_maxZoom = 1.0f;
	glm::vec3 m_zeroCamPos{ 0, 100, 0 }; // Start pos of camera

	bool m_skewTReady{ false };
	bool m_skewTConfirmed{ false };
	bool m_usingHodograph{ false };

	skewTParams m_currentParam;
	bool m_editing{ false };
	int m_editingPoint{ -1 };

	// QOL values
	bool m_cursorActive{ false };
	float m_cursorSize = 0.5f;
	const float m_offset = 0.05f; // Create offset so we do not set the pos on top of the other one
	float m_skewed = glm::tan(glm::radians(45.0f));
	float m_standardIncreaseWind = 0.1f; // Standard increase of wind in the hodograph after placing a point in km
};

