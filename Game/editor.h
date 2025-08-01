#pragma once
#include <memory>

class environment;
struct gridDataSky;
struct gridDataGround;
struct gridDataBounds;
class editor
{
public:

	editor(environment::gridDataSky& skyData, environment::gridDataGround& groundData, int(&height)[GRIDSIZEGROUND], float(&debug1)[GRIDSIZESKY], float(&debug2)[GRIDSIZESKY], float(&debug3)[GRIDSIZESKY]);
	~editor() {};
	void init();

	void update();
	void viewData();
	void keyBased();

	//Get and set
	bool getEditMode() { return m_editMode; }
	int getStep() { return m_simulationStep;  }
	void setStep(int value) { m_simulationStep = value; }
	bool getSimulate() { return m_simulationActive; }
	float getSpeed() { return m_simulationSpeed; }
	void setDeltaTime(float dt) { m_deltatime = dt; }
	bool changedGround() { return m_changedGround; }
private:
	//Main functions
	void panel();
	void editMode();

	//ImGui
	void mainButtons();
	void viewParamInformation();
	void setView();
	void editModeParams();
	void vectorArrow();

	//View
	void viewSky();
	void viewGround();
	void viewSkewT();
	void renderVelSquare(glm::vec2 vel, const int x, const int y);
	void viewBrush();
	void viewToolTipData();

	//Editor
	void applyBrush();

	void setValueOfParam(const int index, const int param, const bool add, const float value, const float secondValue = 0.0f);
	void setGround(const int index, bool ground);
	void addDataErasedGround(const int x, const int y);

	//Helpers
	void dataToSkewTData(float* temp, float* dew, float* pressures);


	//Variables
	environment::gridDataSky& m_envView;
	environment::gridDataGround& m_groundView;

	int (&m_groundHeight)[GRIDSIZEGROUND];

	float (&m_debugArray0)[GRIDSIZESKY];
	float (&m_debugArray1)[GRIDSIZESKY];
	float (&m_debugArray2)[GRIDSIZESKY];

	float m_deltatime = 1.0f / 60.0f;

	bool m_editMode{ false };

	int m_viewParamSky{ 0 };
	int m_viewParamGround{ 0 };
	int m_editParamSky{ 0 };

	int m_mousePointingIndex{ 0 };
	bool m_simulationActive{ false };
	int m_simulationStep{ 0	};
	float m_simulationSpeed{ 1.0f };
	bool m_changedGround = false;
	int m_skewTidx = GRIDSIZESKYX / 2;

	//Brush variables
	bool m_brushing = false;
	float m_brushSize{ 1.0f };
	float m_brushIntensity{ 1.0f }; //TODO: change to parameter specific
	float m_brushSmoothness{ 1.0f };
	glm::vec2 m_brushDir{ 0,1 };
	bool m_groundErase = false;
};

