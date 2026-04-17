#pragma once
#include <memory>
#include "game.h"
#include "config.h"
#include <glm/glm.hpp>
#include "tracing.h"

class environment;
class environmentGPU;
struct gridDataSky;
struct gridDataGround;

class editor
{
public:


	//Could expand into multiple structs/refences from other classes.
	editor(envDebugData* _envDebugData);
	~editor();
	void setColors();
	//We also include m_pressures
	void setIsentropics(float* isentropicTemps, float* isentropicVapor, float* pressure);

	void update(float dt);
	void panel();
	void viewData();

	//Get and set
	bool getEditMode() { return m_editMode; }
	int getStep() { return m_simulationStep; }
	void setStep(int value) { m_simulationStep = value; }
	bool getSimulate() { return m_simulationActive; }
	float getSpeed() { return m_simulationSpeed; }
	bool changedGround() { return m_changedGround; }

	void setDebugValueNum(const float* array, const int num);
	void GPUSetEnv(void* _sky, void* _ground, int* _groundHeight, float* ps);
	void setTime(float sec) { m_time = sec; }
	void setLongitude(float longitude) { m_longitude = longitude; }
	void setDay(int day) { m_day = day; }
	void getTime(float &time) 
	{
		if (m_timeChanged) 
		{
			m_timeChanged = false;
			time = m_time;
		}
	}
	float getLongitude() {return m_longitude;}
	int getDay() {return m_day;}
	bool getDiurnalCyclePaused() { return m_pauseDiurnal; }
	float getSunStrength() { return m_sunStrength; }

private:
	//Main functions
	void editMode();
	void cameraControl();
	void setVariables();

	//ImGui
	void mainButtons();
	void viewParamInformation();
	void setView();
	void setSlice();
	void editModeParams();
	void editModeParamsSun();
	int chooseDateDay();
	void vectorArrow();
	void viewImguiData();
	void setSkewTData();
	void skewTTexture();
	void dataClassView();

	//View
	void viewBackground();
	void viewSky();
	void viewGround();
	void viewSkewT();
	void viewBrush();
	void viewSelection();
	void viewToolTipData();
	void viewPickerData();
	void viewMicroPhysGraph();

	//Editor
	void applyBrush();
	void applySelect();
	void usePicker();

	void resetValues();

	void setValueOfParam(const int index, const parameter param, const bool add, const float value, const float secondValue = 0.0f, const float thirdValue = 0.0f);
	void setGround(const int index, bool ground);
	void addDataErasedGround(const int x, const int y);

	//Helpers
	void dataToSkewTData(float* temp, float* dew, float* pres);
	glm::vec2 getMinMaxVaueParam(parameter param);
	const char* getFormatParam(parameter param, int& flagOutput);
	glm::vec3 getValueParam(const int index, parameter param);
	int getDaysInMonth(int month);
	void dayToMonthDay(int dayOfYear, int& month, int& dayOfMonth);
	void setSliceMinMax(bool fullView);

	//Variables
	envDebugData* m_envData;

	// Ray tracer
	tracing tracerObj = tracing();

	float m_deltatime = 1.0f / 60.0f;

	bool m_editMode{ false };
	bool m_skewTSettings{ false };
	bool m_dataViewer{ false };

	parameter m_viewParamSky{ POTTEMP };
	int m_viewParamGround{ 0 };
	parameter m_editParamSky{ POTTEMP };

	int m_mousePointingIndex{ 0 };
	bool m_selectionInGrid{ false };
	glm::ivec3 m_mousePointingPos{ 0 };
	bool m_simulationActive{ false };
	int m_simulationStep{ 0 };
	float m_simulationSpeed{ 1.0f };
	bool m_changedGround = false;
	int m_skewTidx = GRIDSIZESKYX / 2;
	glm::ivec3 m_skewTPos{ 0 };

	// Viewing Settings
	bool m_viewSlice{ false };
	bool m_viewGround{ true };
	// X = 0, Y = 1, Z = 2
	int m_viewSliceCoord{ 0 };
	int m_atSliceViewSlice{ 0 };
	int m_minViewX = 0;
	int m_minViewY = 0;
	int m_minViewZ = 0;
	int m_maxViewX = GRIDSIZESKYX;
	int m_maxViewY = GRIDSIZESKYY;
	int m_maxViewZ = GRIDSIZESKYZ;

	//General apply variables
	float m_applyValue{ 1.0f };
	glm::vec3 m_valueDir{ 0,0,1 };

	//Brush variables
	bool m_brushing = false;
	float m_brushSize{ 1.0f };
	float m_brushIntensity{ 1.0f }; //TODO: change to parameter specific
	float m_brushSmoothness{ 1.0f };
	bool m_groundErase = false;

	//Select variables
	glm::ivec3 m_saveSelectPos{0};
	bool m_selectReset{ false };
	bool m_selecting{ false };
	glm::ivec3 m_corners[2]{};
	bool m_microPhysSelect{ false };
	bool m_justViewSelection{ false };


	//Camera variables
	glm::vec3 MousePos3D{};
	glm::vec3 SaveMousePos{ 0.0f };
	glm::vec2 Save2DPos{ 0.0f };
	float roll = 0.0f, pitch = 0.0f;
	float SaveRoll = 0.0f, SavePitch = 0.0f;
	float MouseWheel = 0;

	//Diurnal cycle variables
	float m_time = 43200.0f; //0 to 86.400 time in seconds
	const float m_dayLightDuration = 14.0f; //TODO: should be calculated using longitude and day
	const float m_hourOfSunrise = 6.0f;
	bool m_timeChanged = false;
	float m_longitude = 52.37f; //Longitude on earth, 52.37 is Amsterdam
	int m_day = 130; //Day of the year
	float m_sunStrength = 1.0f;
	bool m_pauseDiurnal = false;
	glm::vec3 m_backGroundColor{};

	//Temp
	unsigned int texture = 0;

};

//Stores all the refence variables from environment
struct envDebugData
{
#if USE_GPU
	envDebugData()  // GPU data is filled after each tick
	{
	}

	//Variables
	environment::gridDataSky m_envView;
	environment::gridDataGround m_groundView;

	int m_groundHeight[GRIDSIZEGROUND]{};

#else
	envDebugData(environment::gridDataSky& skyData, environment::gridDataGround& groundData, int(&height)[GRIDSIZEGROUND])
		: m_envView(skyData), m_groundView(groundData), m_groundHeight(height)
	{
	}

	//Variables
	environment::gridDataSky& m_envView;
	environment::gridDataGround& m_groundView;

	int(&m_groundHeight)[GRIDSIZEGROUND];
#endif



	float m_debugArray0[GRIDSIZESKY]{};
	float m_debugArray1[GRIDSIZESKY]{};
	float m_debugArray2[GRIDSIZESKY]{};

	float m_envTemp[GRIDSIZESKYY]{};
	float m_envVapor[GRIDSIZESKYY]{};
	float m_envPressure[GRIDSIZESKYY]{};
};
