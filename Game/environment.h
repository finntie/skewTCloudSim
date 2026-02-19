#pragma once
#include <vector>
#include "half/half.hpp"
#include <glm/glm.hpp>
#include "config.h"

struct envDebugData;
class environmentGPU;

class environment
{
public:

	//TODO: doubles are used for the temps, due to only 14 million precision between biggest and smallest number stored.
	struct gridDataSky //28 bytes, using floats: 40
	{
		/*half_float::half*/float Qv[GRIDSIZESKY]{ 0.01f }; //  Mixing Ratio of Water Vapor
		/*half_float::half*/float Qw[GRIDSIZESKY]{ 0.01f }; //	Mixing Ratio of	Liquid Water
		/*half_float::half*/float Qc[GRIDSIZESKY]{ 0.01f }; //	Mixing Ratio of Ice 
		/*half_float::half*/float Qr[GRIDSIZESKY]{ 0.01f }; //	Mixing Ratio of Rain
		/*half_float::half*/float Qs[GRIDSIZESKY]{ 0.01f }; //	Mixing Ratio of Snow
		/*half_float::half*/float Qi[GRIDSIZESKY]{ 0.01f }; //	Mixing Ratio of Ice (precip)
		float potTemp[GRIDSIZESKY]{ 1.0f };			 // Potential temperature
		glm::vec3 velField[GRIDSIZESKY]{};				 // Velocity field (fluid sim)
		float pressure[GRIDSIZESKY]{};

		//float dummy{ 0.0f }; //Room for 4 bytes?		
	};

	//TODO: do we want halfs or not? precision lies on about 6e-8f; 
	struct gridDataGround //16 bytes, using floats: 32
	{
		/*half_float::half*/float Qrs[GRIDSIZEGROUND]{0.01f }; // Subsurface water content
		/*half_float::half*/float Qgr[GRIDSIZEGROUND]{0.01f }; // Rain content
		/*half_float::half*/float Qgs[GRIDSIZEGROUND]{0.01f }; // Snow content
		/*half_float::half*/float Qgi[GRIDSIZEGROUND]{0.01f }; // Ice content
		/*half_float::half*/float P[GRIDSIZEGROUND]{ 1000.0f }; // Ground Pressure
		/*half_float::half*/float t[GRIDSIZEGROUND]{ 0.0f }; // Time since ground was wet
		float T[GRIDSIZEGROUND]{ 0.0f };  // Ground temperature
	};

	environment();
	~environment();

	void init(float* potTemps, glm::vec2* velField, float* Qv, float* groundTemp, float* groundPres, float* pressures);

	void EditorData();

	void Update(float dt, float speed);

	//--------------------------------Ground---------------------------------
	float irridiance();
	float groundCoverageFactor(const int index);
	void updateGroundTemps(const float dt, const int index, const float Irradiance, const float cloudCoverage);
	void advectMicroPhysicsGround(const float dt, const int index);
	void updateMicroPhysicsGround(const float dt, const int index, const float Tair, const float irr, const float c, const float density);
	float calculateSumPhaseHeatGround(const int i);


	//----------------------------------Sky----------------------------------
	void diffuseAndAdvectTemp(const float dt);
	void getInterpolValueTemp(float* arrayFull, const glm::vec2 Ppos, float& output);
	//type: rain = 1, snow = 2, ice = 3
	void diffuseAndAdvect(const float dt, float* array, std::vector<float>& density, bool vapor = false, const int fallVelocityType = 0);
	void interPolatePrecip(const float dt, float* array, const int fallVelocityType);
	bool getInterpolValue(float* array,const glm::vec2 Ppos, const bool Vapor, float& output);
	void PPMWAdvect(float* array, float* defaultVal, const int i, const float dt);
	void PPMWAdvectLR(float* array, float* defaultVal, const int i, const float dt, const bool x);
	float PPMWAdvectFlux(float* array, float* defaultVal, const int i, const float dt, const bool x, const bool right);
	void updateVelocityField(const float dt);
	float calculateBuoyancy(const int index);
	float averageEnvironment(const int index, const int distanceFromidx, const float maxDistance, const bool temp);
	glm::vec2 vorticityConfinement(const int index);
	bool getInterpolVel(glm::vec2 Ppos, bool U, float& output);
	//-----PressureProject-----
	void pressureProjectVelField();
	void calculatePresProj(std::vector<float>& pressureProj);
	void calculateDivergence(std::vector<float>& output);
	void calculatePrecon(std::vector<float>& output, std::vector<glm::ivec3>& A);
	void applyPreconditioner(std::vector<float>& precon, std::vector<float>& r, std::vector<glm::ivec3>& A, std::vector<float>& storageQ, std::vector<float>& output);
	void applyA(std::vector<float>& s, std::vector<glm::ivec3>& A, std::vector<float>& output);
	/// <summary>Calculates the mass-weighted mean terminal velocity of all types of precip</summary>
	/// <param name = "type"> 1 = rain, 2 = snow, 3 = hail, 4 = all</param>
	/// <returns>x: rain, y: snow, z: ice</returns>
	glm::vec3 calculateFallingVelocity(const int index, const float density, const int type);
	void updateMicroPhysics(const float dt, const int index, const float T, const float density);
	float calculateSumPhaseHeat(const float dt, const int index, const float Temp);
	void computeHeatTransfer(const int index, const float sumHeat);

	/// <summary> Get UV from the velocity field which is in MAC grid</summary>
	glm::vec2 getUV(const int index);
	glm::vec3 getUV(const glm::vec3* velField, const int x, const int y, const int z);

	/// <summary>Get ambient temp at height. Using avaraged lapse rate between 5 and 2 km. </summary>
	float getIsentropicTemp(const float y);
	float getIsentropicVapor(const float y);
	float curl(const int index, bool raw = false);
	float div(const int index);
	glm::vec2 lap(const int index);
	bool outside(const int i); //For index, does not work on x bounds
	bool outside(const float x, const float y); //For coords
	bool isGround(int i);
	bool isGround(int x, int y);
	bool isGroundLevel(int i);
	bool isGroundLevel(int x, int y);

	void computeNeighArray();

	Neigh m_NeighData[GRIDSIZESKY]; //Neighbour data
	envDebugData* getDebugData();

private:
	gridDataSky m_envGrid;
	gridDataGround m_groundGrid;

	float m_time = 43200.0f; //0 to 86.400 time in seconds
	const float m_dayLightDuration = 14.0f;
	const float m_hourOfSunrise = 6.0f;
	float m_speed = 1.0f;
	float m_longitude = 52.37f; //Longitude on earth, 52.37 is Amsterdam
	int m_day = 130; //Day of the year
	float m_sunStrength = 1.0f;
	bool m_pauseDiurnal = false;

	float m_condens = 0.0f; //Heat from condensation
	float m_freeze = 0.0f; //Heat from freezing
	float m_depos = 0.0f; //Heat from deposition (gas to solid)

	float m_isenTropicTemps[GRIDSIZESKYY]{ 0.0f };
	float m_isenTropicVapor[GRIDSIZESKYY]{ 0.0f };
	float m_pressures[GRIDSIZESKYY]{ 0.0f };
	float m_defaultVel[GRIDSIZESKYY]{ 0.0f };
	int m_GHeight[GRIDSIZEGROUND]{ 0 };
	float m_dummyArray[GRIDSIZESKYY]{ 0 };

	float velocityX[GRIDSIZESKY];
	float velocityY[GRIDSIZESKY];

	float m_debugArray0[GRIDSIZESKY]{ 0.0f };
	float m_debugArray1[GRIDSIZESKY]{ 0.0f };
	float m_debugArray2[GRIDSIZESKY]{ 0.0f };
};

