#pragma once

#include <cuda_runtime.h> 
struct Neigh;
enum parameter : uint16_t;
struct microPhysicsParams;
struct envDebugData;

class environmentGPU
{
public:

	struct gridDataSkyGPU // 36 bytes
	{
		float* Qv; //  Mixing Ratio of Water Vapor
		float* Qw; //	Mixing Ratio of	Liquid Water
		float* Qc; //	Mixing Ratio of Ice 
		float* Qr; //	Mixing Ratio of Rain
		float* Qs; //	Mixing Ratio of Snow
		float* Qi; //	Mixing Ratio of Ice (precip)
		float* potTemp;			 // Potential temperature
		float* velfieldX;
		float* velfieldY;
	};

	struct gridDataGroundGPU // 28 bytes
	{
		float* Qrs; // Subsurface water content
		float* Qgr; // Rain content
		float* Qgs; // Snow content
		float* Qgi; // Ice content
		float* P; // Ground Pressure
		float* t; // Time since ground was wet
		float* T;  // Ground temperature
	};

	environmentGPU();
	~environmentGPU();

	void init(float* potTemps, glm::vec2* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures);

	void updateGPU(float dt, const float speed);

	//----------------MicroPhysics--------------

	void microPhysicsGroundGPU(const float dt, const float speed, const float irradiance);
	void microPhysicsSkyGPU(const float dt, const float speed);
	//------------------------------------------


	//-----------------Diffusing----------------

	// type: temp = 0, vapor = 1, velX = 2, velY = 3, default = 4
	void diffuseGPU(float* diffuseArray, int type, const float dt);
	//------------------------------------------


	//-----------------Advecting----------------

	void advectGroundWaterGPU(const float dt, const float speed);
	void setTempsAtGround(const float dt, const float speed);
	void advectAddDensity(float* array, const float* defaultVal, const float dt, const bool density);
	void advectPPMWGPU(float* array, const float* defaultVal, const float dt);
	// fallVelType: rain = 0, snow = 1, hail = 2
	void advectPrecip(float* array, const int fallVelType, const float dt);
	//------------------------------------------


	//-------------Pressure Project-------------
	void pressureProject();
	void calculatePressureProject(float* outputPressure);
	//------------------------------------------


	//-------------------Other------------------

	void resetValues();
	void editorDataGPU();
	float irridianceGPU();
	void groundCoverageFactorGPU();
	void updateGroundTempsGPU(const float dt, const float speed, const float irridiance);
	void calculateBuoyancy(const float dt);
	bool isGround(int x, int y);
	float* getParamArray(parameter type, const bool windX = true);
	//------------------------------------------


	//-------------------Outside------------------

	void prepareBrushGPU(parameter paramType, const float brushSize, const float2 mousePos, const float2 extras,
		const float brushSmoothnes, const float dt, const float brushIntensity, const float applyValue, const float2 valueDir, const bool groundErase);
	void resetParameterGPU(parameter paramType);
	//------------------------------------------

	//-------------------Get/Set------------------

	envDebugData* getDebugData();
	//gridDataSkyGPU& getEnvGridGPU() { return m_envGrid; }
	//gridDataGroundGPU& getGroundGridGPU() { return m_groundGrid; }
	//float* getIsenTropicTemp() { return m_isentropicTemp; }
	//float* getIsenTropicVapor() { return m_isentropicVapor; }
	//float* getDefaultWind() { return m_defaultVel; }

	//------------------------------------------


private:



	gridDataSkyGPU m_envGrid;
	gridDataGroundGPU m_groundGrid;

	float m_time = 43200.0f; //0 to 86.400 time in seconds
	const float m_dayLightDuration = 14.0f;
	const float m_hourOfSunrise = 6.0f;
	float m_longitude = 52.37f; //Longitude on earth, 52.37 is Amsterdam
	int m_day = 130; //Day of the year
	float m_sunStrength = 1.0f;
	bool m_pauseDiurnal = false;

	//GPU variables
	float* m_array;
	float* m_outputArray;
	float* m_defaultVal;
	float* m_density;

	Neigh* m_neighbourData;
	microPhysicsParams* m_microPhysRes;//Used for microphysics

	int* m_GHeight;
	int* m_dummyGHeight;

	float* m_pressures;
	float* m_defaultVel;
	float* m_isentropicTemp;
	float* m_isentropicVapor;
	float* m_dummyArray;
	float* m_dummyArraySky2;

	float* m_dummyArrayGround;
	float* m_dummyArrayGround2;

	float* m_condens;
	float* m_depos;
	float* m_freeze;
	
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

	bool m_groundChanged{ true };
	float* m_precon; //Precon (pressure projection)
	char3* m_A; //A matrix (pressure projection)

	//CPU storage
	float m_velXCPU[GRIDSIZESKY];
	float m_velYCPU[GRIDSIZESKY];

};