#pragma once
#include "core/ecs.hpp"
#include "tools/inspectable.hpp"

#include "half/half.hpp"
#include <glm/glm.hpp>

#define GRIDSIZESKYX 32
#define GRIDSIZESKYY 128

#define GRIDSIZESKY (GRIDSIZESKYX * GRIDSIZESKYY)
#define GRIDSIZEGROUND (GRIDSIZESKYX)
#define VOXELSIZE 16.0f //Meters

class environment : public bee::System, public bee::IPanel
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
		double potTemp[GRIDSIZESKY]{ 1.0f };			 // Potential temperature
		glm::vec2 velField[GRIDSIZESKY]{};				 // Velocity field (fluid sim) needs to be changed to vec3 later

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
		double T[GRIDSIZEGROUND]{ 0.0f };  // Ground temperature
	};

	environment();
	~environment();

	void init(double* potTemps, glm::vec2* velField, float* Qv, double* groundTemp, float* groundPres);

#ifdef BEE_INSPECTOR
	void OnPanel() override;
	std::string GetName() const override;
	std::string GetIcon() const override;
#endif

	void DebugRender(float dt);

	void Update(float dt) override;

	//--------------------------------Ground---------------------------------
	float groundCoverageFactor(const int index);
	void updateGroundTemps(const float dt, const int index, const float Irradiance, const float cloudCoverage);
	void updateMicroPhysicsGround(const float dt, const int index);
	float calculateSumPhaseHeatGround(const int i, const float pQgr, const float pQgs, const float pQgi);


	//----------------------------------Sky----------------------------------
	void diffuseAndAdvectTemp(const float dt, double* array);
	void getInterpolValueTemp(double* arrayFull, const glm::vec2 Ppos, double& output);
	//type: rain = 1, snow = 2, ice = 3
	void diffuseAndAdvect(const float dt, float* array, bool vapor = false, const int fallVelocityType = 0);
	bool getInterpolValue(float* array,const glm::vec2 Ppos, const bool Vapor, float& output);
	void updateVelocityField(const float dt);
	float calculateBuoyancy(const int index, const float* m_pressures);
	float averageEnvironment(const int index, const int distanceFromidx, const float maxDistance, const bool temp);
	//void backTracing(const float dt, const int index, const float fallingVelocity);
	//float trilinearSampling(const glm::vec2 pos, half_float::half* array);
	glm::vec2 vorticityConfinement(const int index);
	bool getInterpolVel(glm::vec2 Ppos, bool U, float& output);
	//-----PressureProject-----
	void pressureProjectVelField(const float dt);
	void calculatePresProj(std::vector<float>& pressureProj);
	void calculateDivergence(std::vector<float>& output);
	void calculatePrecon(std::vector<float>& output, std::vector<glm::ivec3>& A);
	void applyPreconditioner(std::vector<float>& precon, std::vector<float>& r, std::vector<glm::ivec3>& A, std::vector<float>& storageQ, std::vector<float>& output);
	void applyA(std::vector<float>& s, std::vector<glm::ivec3>& A, std::vector<float>& output);
	/// <summary>Calculates the mass-weighted mean terminal velocity of all types of precip</summary>
	/// <returns>x: rain, y: snow, z: ice</returns>
	glm::vec3 calculateFallingVelocity(const float dt, const int index, const float density);
	void updateMicroPhysics(const float dt, const int index, const float* m_pressures, const float T, const float density);
	float calculateSumPhaseHeat(const int index, const float Temp, const float pQv, const float pQw, const float pQc, const float pQr, const float pQs, const float pQi);
	void computeHeatTransfer(const float dt, const int index, const float sumHeat);

	/// <summary> Get UV from the velocity field which is in MAC grid</summary>
	glm::vec2 getUV(const int index);
	/// <summary>Get ambient temp at height. Using avaraged lapse rate between 5 and 2 km. </summary>
	float getIsentropicTemp(const float y);
	float curl(const int index, bool raw = false);
	float div(const int index);
	glm::vec2 lap(const int index);
	bool outside(const int i); //For index, does not work on x bounds
	bool outside(const float x, const float y); //For coords
	bool isGround(int i);
	bool isGround(int x, int y);

	void computeNeighArray();
	void setDebugArray(std::vector<float>& s, const int index = 0);

private:
	

	gridDataSky m_envGrid;
	gridDataGround m_groundGrid;

	//TODO: Convert to 60 seconds, 3600 for minute and 86.400 for day
	float m_time = 12.0f; //0 to 24
	float m_dayLightDuration = 10.0f;
	float m_hourOfSunrise = 6.0f;
	float m_speed = 1.0f;

	float m_isenTropicTemps[GRIDSIZESKYY]{ 0.0f };
	float m_pressures[GRIDSIZESKY]{ 0.0f };
	int m_GHeight[GRIDSIZEGROUND]{ 0 };

	//Storage array for setting boundary conditions easier.
	enum envType
	{
		SKY, OUTSIDE, GROUND
	};
	struct Neigh 
	{
		envType left, right, up, down;
	};
	Neigh m_NeighData[GRIDSIZESKY]; //Neighbour data


	float m_debugArray0[GRIDSIZESKY]{ 0.0f };
	float m_debugArray1[GRIDSIZESKY]{ 0.0f };
	float m_debugArray2[GRIDSIZESKY]{ 0.0f };

	//Debug
	//int debugViewSky = 0;
	//int debugViewGround = 0;
	//int m_debugEditParam = 0;
	//
	//int mousePointingIndex = 0;
	//bool simulationActive = false;
	//int simulationStep = 0;
};

