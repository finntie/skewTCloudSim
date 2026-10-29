#pragma once

#include "environment.hpp"
#include "config.hpp"

#include <glm/glm.hpp>
#include <unordered_map>

class dataClass;
struct microPhysicsParams;
struct envDebugData;
struct hipGraphExec;
struct float4;
struct simParams;


void runTest();
class environmentGPU
{
public:

	environmentGPU(const simParams& params);
	~environmentGPU();

	// Makes space for all data, should happen before init
	void allocateData();

	// Frees all data that was allocated
	void freeData();

	void init(float* potTemps, glm::vec3* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures, float* smallPressure);

	// Resizes all the data, removed already allocated data
	void resize();

	void updateGPU(float dt, const float speed);

	//----------------MicroPhysics--------------

	void microPhysicsGroundGPU(const float dt, const float speed, const float irradiance);
	void microPhysicsSkyGPU(const float dt, const float speed);
	//------------------------------------------


	//-----------------Diffusing----------------

	// type: temp = 0, vapor = 1, velX = 2, velY = 3, velZ = 4, default = 5
	void diffuseGPU(float* diffuseArray, int type, const float dt);
	void initDiffuseGPUGraph(float* diffuseArray, int type);
	//------------------------------------------


	//-----------------Advecting----------------

	void advectGroundWater(const float dt, const float speed);
	void setTempsAtGround(const float dt, const float speed);
	void advectPPMWGPU(float* array, const float* defaultVal, boundsEnv boundsVal, const float dt);
	void initAdvectPPMWGPUGraph(float* array, const float* defaultVal, boundsEnv boundsVal);
	// fallVelType: rain = 0, snow = 1, hail = 2
	void advectPrecip(float* array, const int fallVelType, const float dt);
	//------------------------------------------


	//-------------Pressure Project-------------
	void pressureProject(const float dt);
	void calculatePressureProject(const float dt);
	void initPressureProjectGraph();
	//------------------------------------------


	//-------------------Other------------------

	float irridianceGPU();
	void groundCoverageFactor();
	void updateGroundTemps(const float dt, const float speed, const float irridiance);
	void calculateBuoyancy(const float dt);
	bool isGround(int x, int y);
	float* getParamArray(parameter type);
	//------------------------------------------


	//-------------------Outside------------------

	void updateOutOfSyncGround();
	void resetGroundValues();
	void prepareBrushGPU(parameter paramType, const float brushSize, const glm::ivec3 mousePos,
		const float brushSmoothnes, const float dt, const float brushIntensity, const float applyValue, const glm::vec3 valueDir, const bool groundErase);
	void prepareSelectionGPU(parameter paramType, const glm::ivec3 minPos, const glm::ivec3 maxPos, const float applyValue, const glm::vec3 valueDir, const bool groundErase);
	void resetParameterGPU(parameter paramType);
	//------------------------------------------

	//-------------------Get/Set------------------
	// 
	gridDataSkyGPU& getRenderData() { return m_envGrid; }
	void setHostData(envDebugData& outputCPUData, bool setIsenTropics);
    void setHostSettingsData(float& time, int day, float sunStrength, float longitude, bool pauseDiurnal);
	void setMicroPhysDataValues(glm::ivec3 minPos, glm::ivec3 maxPos, bool active);
    void retrieveMicroPhysResults(microPhysEnvValuesSingle& singleParamsCPU);
    void getDebugArrayValues(envDebugData& outputCPUData);
	void synchronizeSimStream();
	//gridDataSkyGPU& getEnvGridGPU() { return m_envGrid; }
	//gridDataGroundGPU& getGroundGridGPU() { return m_groundGrid; }
	//float* getIsenTropicTemp() { return m_isentropicTemp; }
	//float* getIsenTropicVapor() { return m_isentropicVapor; }
	//float* getDefaultWind() { return m_defaultVel; }

	//------------------------------------------


private:



	gridDataSkyGPU m_envGrid{};
	gridDataGroundGPU m_groundGrid{};
	simInfo simKernelInfo{};
	const simParams& m_params;

	bool canFillAll{ false }; // If we can fit all threads within the simulation or not

	bool allocatedData{ false };

	float m_time = 43200.0f; //0 to 86.400 time in seconds
	static constexpr float m_dayLightDuration = 14.0f;
	static constexpr float m_hourOfSunrise = 6.0f;
	float m_longitude = 52.37f; //Longitude on earth, 52.37 is Amsterdam
	int m_day = 130; //Day of the year
	float m_sunStrength = 1.0f;
	bool m_pauseDiurnal = false;

	// Graphs
    std::unordered_map<float*, hipGraphExec*> m_diffuseExecutionGraphs{};
    std::unordered_map<float*, hipGraphExec*> m_advectExecutionGraphs{};
    hipGraphExec* m_pressureprojectExecutionGraph{nullptr};
	//GPU variables
	float* m_array;
	float* m_outputArray;
	float* m_storPres; // Storage for previous pressure outcome
	float* m_density;

	float* m_oldDensityAir;
	float* m_densityAir;// kg/m3 Used to pressure project

	Neigh* m_neighbourData;
	microPhysicsParams* m_microPhysRes;//Used for microphysics

	int* m_GHeight;
	int* m_dummyGHeight;

	float* m_defaultPressure;
	float* m_defaultVelX;
	float* m_defaultVelZ;
	float* m_isentropicTemp;
	float* m_isentropicVapor;
	float* m_dummyArray;
	float* m_dummyArraySky2;

	float* m_dummyArrayGround;
	float* m_dummyArrayGround2;

	float* m_condens;
	float* m_depos;
	float* m_freeze;
	
	float* m_precon; //Precon (pressure projection)
	float4* m_A; //A matrix (pressure projection)

	//Single values
	float* m_singleStor0;
	float* m_sigma0;
	float* m_sigma1;
	int* m_firstValid;
	bool* m_storBool;

	//Extra Storage
	float* m_stor0;
	float* m_stor1;
	float* m_stor2;

	// extra variables
	bool m_groundChanged{ true };
	bool m_updateGround{ true }; // Ground has changed, but has to updateInput variables still
	bool m_updatingSimulation{ false }; // Shared data to check if simulation is running or finished
    bool m_microPhysDataActive{false};
    glm::ivec3 m_microPhysMinPos{-1, -1, -1};
    glm::ivec3 m_microPhysMaxPos{-1, -1, -1};

	// Debug arrays, point to any array already existing
    float* m_debugArray0 = nullptr, *m_debugArray1 = nullptr, *m_debugArray2 = nullptr;
};

