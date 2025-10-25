#pragma once
#include <memory>

class environment;
struct gridDataSky;
struct gridDataGround;
struct gridDataBounds;
class editor
{
public:

	enum parameter
	{
		POTTEMP, QV, QW, QC, QR, QS, QI, WIND, GROUND, DEBUG1, DEBUG2, DEBUG3
	};

	//Could expand into multiple structs/refences from other classes.
	editor(envDebugData* _envDebugData);
	~editor();
	void setColors();
	//We also include m_pressures
	void setIsentropics(float* isentropicTemps, float* isentropicVapor, float* m_pressures);

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

private:
	//Main functions
	void editMode();
	void cameraControl();
	void setVariables();

	//ImGui
	void mainButtons();
	void viewParamInformation();
	void setView();
	void editModeParams();
	void vectorArrow();
	void viewImguiData();
	void setSkewTData();

	//View
	void viewSky();
	void viewGround();
	void viewSkewT();
	void renderVelSquare(glm::vec2 vel, const int x, const int y);
	void viewBrush();
	void viewSelection();
	void viewToolTipData();
	void viewPickerData();

	//Editor
	void applyBrush();
	void applySelect();
	void usePicker();

	void setValueOfParam(const int index, const parameter param, const bool add, const float value, const float secondValue = 0.0f);
	void setGround(const int index, bool ground);
	void addDataErasedGround(const int x, const int y);

	//Helpers
	void dataToSkewTData(float* temp, float* dew);
	glm::vec2 getMinMaxVaueParam(parameter param);
	const char* getFormatParam(parameter param, int& flagOutput);
	glm::vec2 getValueParam(const int index, parameter param);

	//Variables
	envDebugData* m_envData;

	float m_deltatime = 1.0f / 60.0f;

	bool m_editMode{ false };
	bool m_skewTSettings{ false };


	parameter m_viewParamSky{ POTTEMP };
	int m_viewParamGround{ 0 };
	parameter m_editParamSky{ POTTEMP };

	int m_mousePointingIndex{ 0 };
	bool m_simulationActive{ false };
	int m_simulationStep{ 0 };
	float m_simulationSpeed{ 1.0f };
	bool m_changedGround = false;
	int m_skewTidx = GRIDSIZESKYX / 2;

	//General apply variables
	float m_applyValue{ 1.0f };
	glm::vec2 m_valueDir{ 0,1 };

	//Brush variables
	bool m_brushing = false;
	float m_brushSize{ 1.0f };
	float m_brushIntensity{ 1.0f }; //TODO: change to parameter specific
	float m_brushSmoothness{ 1.0f };
	bool m_groundErase = false;

	//Select variables
	glm::vec2 m_saveSelectPos{ 0,0 };
	glm::vec2 m_toSelectPos{ 0,0 };
	bool m_selectReset{ false };
	bool m_selecting{ false };


	//Camera variables
	glm::vec3 MousePos3D{};
	glm::vec3 SaveMousePos = glm::vec3(0);
	glm::vec2 Save2DPos = glm::vec2(0);
	float roll = 0.0f, pitch = 0.0f;
	float SaveRoll = 0.0f, SavePitch = 0.0f;
	float MouseWheel = 0;
};

//Stores all the refence variables from environment
struct envDebugData
{
	envDebugData(environment::gridDataSky& skyData, environment::gridDataGround& groundData, int(&height)[GRIDSIZEGROUND], float(&debug1)[GRIDSIZESKY], float(&debug2)[GRIDSIZESKY], float(&debug3)[GRIDSIZESKY])
		: m_envView(skyData), m_groundView(groundData), m_groundHeight(height), m_debugArray0(debug1), m_debugArray1(debug2), m_debugArray2(debug3)
	{
	}

	//Variables
	environment::gridDataSky& m_envView;
	environment::gridDataGround& m_groundView;

	int(&m_groundHeight)[GRIDSIZEGROUND];

	float(&m_debugArray0)[GRIDSIZESKY];
	float(&m_debugArray1)[GRIDSIZESKY];
	float(&m_debugArray2)[GRIDSIZESKY];

	float m_envTemp[GRIDSIZESKYY]{};
	float m_envVapor[GRIDSIZESKYY]{};
	float m_envPressure[GRIDSIZESKYY]{};
};
